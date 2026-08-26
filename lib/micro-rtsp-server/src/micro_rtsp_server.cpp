#include <esp32-hal-log.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <micro_rtsp_server.h>
#include <micro_rtsp_structs.h>
#include <jpg.h>

// Check client connections every 10 milliseconds
#define CHECK_CLIENT_INTERVAL 10
// UDP port used for the RTP video stream (and +1 for RTCP)
#define RTP_UDP_PORT 6970
// UDP port used for the RTP audio stream (and +1 for RTCP)
#define AUDIO_UDP_PORT 6972
// Poll the audio source every 20 milliseconds (160 samples at 8 kHz)
#define AUDIO_UPDATE_INTERVAL 20

micro_rtsp_server::micro_rtsp_server(micro_rtsp_source &source, micro_rtsp_audio_source *audio_source /*= nullptr*/)
    : source_(source),
      audio_source_(audio_source),
      srtp_enabled_(false),
      frame_interval_(200),
      next_frame_update_(0),
      next_audio_update_(0),
      next_check_client_(0),
      rtp_udp_port_(RTP_UDP_PORT),
      audio_udp_port_(AUDIO_UDP_PORT)
{
}

micro_rtsp_server::~micro_rtsp_server()
{
    end();
}

void micro_rtsp_server::begin(unsigned short port /*= 554*/)
{
    WiFiServer::begin(port);
    log_i("RTSP server listening on TCP port %u", port);

    // Open the UDP sockets used for RTP transport
    rtp_udp_.begin(rtp_udp_port_);
    audio_udp_.begin(audio_udp_port_);
    log_i("RTP UDP sockets on port %u (video) and %u (audio)", rtp_udp_port_, audio_udp_port_);
}

void micro_rtsp_server::end()
{
    WiFiServer::end();
    rtp_udp_.stop();
    audio_udp_.stop();
    clients_.clear();
}

void micro_rtsp_server::loop()
{
    auto now = millis();

    if (next_check_client_ < now)
    {
        log_v("Check for new client");
        next_check_client_ = now + CHECK_CLIENT_INTERVAL;

        // Check if a client wants to connect
        WiFiClient client;
        while ((client = accept()))
        {
            auto c = std::unique_ptr<rtsp_client>(new rtsp_client(client, source_, audio_source_ != nullptr, srtp_enabled_));
            c->set_server_ports(rtp_udp_port_, rtp_udp_port_ + 1);
            clients_.push_back(std::move(c));
            log_i("New RTSP client, total: %d", clients_.size());
        }

        // Check for idle clients
        clients_.remove_if([](std::unique_ptr<rtsp_client> &c)
                           { return !c->connected() || c->stopped(); });

        for (auto &client : clients_)
        {
            client->handle_request();
            // When SRTP was negotiated (server configured or client offered
            // a=crypto), make the streamer protect the outgoing RTP packets.
            if (client->srtp_active() && client->streamer.srtp() == nullptr)
                client->streamer.set_srtp(&client->srtp());
        }
    }

    if (next_frame_update_ < now)
    {
        log_v("Stream frame t=%d", next_frame_update_);
        next_frame_update_ = now + frame_interval_;
        send_video_frame();
    }

    if (audio_source_ != nullptr && next_audio_update_ < now)
    {
        next_audio_update_ = now + AUDIO_UPDATE_INTERVAL;
        send_audio_chunk();
    }
}

void micro_rtsp_server::send_packet(rtsp_client &client, WiFiUDP &udp, const uint8_t *packet, size_t packet_size, uint16_t dest_port)
{
    if (client.tcp_transport())
    {
        // RTP over RTSP/TCP: the packet already starts with the '$' framing header
        client.write(packet, packet_size);
    }
    else
    {
        // RTP over UDP: send the RTP packet without the 4 byte '$' framing header
        udp.beginPacket(client.remoteIP(), dest_port);
        udp.write(packet + rtp_over_tcp_hdr_size, packet_size - rtp_over_tcp_hdr_size);
        udp.endPacket();
    }
}

void micro_rtsp_server::send_video_frame()
{
    bool any_active = false;
    for (auto &client : clients_)
    {
        if (client->active() && client->video_ready())
        {
            any_active = true;
            break;
        }
    }
    if (!any_active)
        return;

    // Get the next JPEG frame from the camera
    source_.update_frame();
    if (source_.data() == nullptr || source_.size() == 0)
        return;

    // Decode the frame to extract the quantization tables and the scan data
    jpg jpg;
    if (!jpg.decode(source_.data(), source_.size()))
    {
        log_e("Unable to decode JPEG frame");
        return;
    }

    // RTP timestamp for the frame, using a 90 kHz clock (RFC 3551)
    const uint32_t timestamp = millis() * 90;

    auto jpg_scan = (uint8_t *)jpg.jpeg_data_start;
    auto jpg_scan_end = (uint8_t *)jpg.jpeg_data_end;

    // Stream the scan data in fragments to every active client
    while (jpg_scan < jpg_scan_end)
    {
        for (auto &client : clients_)
        {
            if (!client->active() || !client->video_ready())
                continue;

            size_t packet_size = 0;
            auto offset = jpg_scan; // per-client copy of the fragment start
            auto packet = client->streamer.create_jpg_packet(
                jpg.jpeg_data_start, jpg.jpeg_data_end, &offset, timestamp,
                jpg.quantization_table_luminance_ ? jpg.quantization_table_luminance_->data : nullptr,
                jpg.quantization_table_chrominance_ ? jpg.quantization_table_chrominance_->data : nullptr,
                packet_size);
            if (packet != nullptr)
            {
                send_packet(*client, rtp_udp_, packet, packet_size, client->video_rtp_port());
                free(packet);
            }
        }

        // All clients consume the same fragment size, advance to the next fragment
        auto remaining = (size_t)(jpg_scan_end - jpg_scan);
        auto fragment_size = std::min(micro_rtsp_streamer::max_payload_size(), remaining);
        jpg_scan += fragment_size;
    }
}

void micro_rtsp_server::send_audio_chunk()
{
    if (audio_source_ == nullptr)
        return;

    bool any_audio = false;
    for (auto &client : clients_)
    {
        if (client->active() && client->audio_ready())
        {
            any_audio = true;
            break;
        }
    }
    if (!any_audio)
        return;

    // Capture the next chunk of (a-law encoded) samples
    if (!audio_source_->update_audio())
        return;

    const uint8_t *data = audio_source_->data();
    const size_t size = audio_source_->size();
    if (data == nullptr || size == 0)
        return;

    for (auto &client : clients_)
    {
        if (!client->active() || !client->audio_ready())
            continue;

        size_t packet_size = 0;
        auto packet = client->streamer.create_audio_packet(data, size, false, packet_size);
        if (packet != nullptr)
        {
            send_packet(*client, audio_udp_, packet, packet_size, client->audio_rtp_port());
            free(packet);
        }
    }
}

micro_rtsp_server::rtsp_client::rtsp_client(const WiFiClient &wifi_client, micro_rtsp_source &source, bool audio_enabled, bool srtp_enabled)
    : WiFiClient(wifi_client), micro_rtsp_requests(audio_enabled, srtp_enabled), streamer(source)
{
}

micro_rtsp_server::rtsp_client::~rtsp_client()
{
    stop();
}

void micro_rtsp_server::rtsp_client::append_request_data(const uint8_t *data, size_t size)
{
    request_buffer.append((const char *)data, size);
}

void micro_rtsp_server::rtsp_client::handle_request()
{
    // Read all available bytes and buffer them
    auto bytes_available = available();
    if (bytes_available > 0)
    {
        std::vector<uint8_t> data(bytes_available);
        auto read = this->read(data.data(), bytes_available);
        if (read > 0)
            append_request_data(data.data(), (size_t)read);
    }

    // Skip RTP/RTCP packets multiplexed on the RTSP connection (RFC 2326 10.12).
    // They start with '$' followed by a channel byte and a 16 bit length.
    while (request_buffer.size() >= rtp_over_tcp_hdr_size && (uint8_t)request_buffer[0] == '$')
    {
        uint16_t length = ((uint16_t)(uint8_t)request_buffer[2] << 8) | (uint8_t)request_buffer[3];
        size_t total = rtp_over_tcp_hdr_size + length;
        if (request_buffer.size() < total)
            break; // wait for the rest of the packet
        log_v("Skipping %u bytes of interleaved data on channel %d", length, (uint8_t)request_buffer[1]);
        request_buffer.erase(0, total);
    }

    // Process complete requests (headers terminated by an empty line, followed
    // by an optional body whose length is given by the Content-Length header)
    for (;;)
    {
        size_t headers_end = request_buffer.find("\r\n\r\n");
        if (headers_end == std::string::npos)
            break;

        // Parse Content-Length from the header portion
        size_t content_length = 0;
        std::string headers = request_buffer.substr(0, headers_end);
        size_t cl = headers.find("Content-Length:");
        if (cl != std::string::npos)
            content_length = strtoul(headers.c_str() + cl + 15, nullptr, 10);

        size_t total = headers_end + 4 + content_length;
        if (request_buffer.size() < total)
            break; // wait for the rest of the body

        auto request = request_buffer.substr(0, total);
        request_buffer.erase(0, total);

        log_i("Request: %s", request.c_str());
        auto response = process_request(request);
        log_i("Response: %s", response.c_str());
        print(response.c_str());
        flush();
    }
}
