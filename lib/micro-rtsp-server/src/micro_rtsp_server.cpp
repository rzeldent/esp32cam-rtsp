#include <esp32-hal-log.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <WiFi.h>

#include <micro_rtsp_server.h>
#include <micro_rtsp_structs.h>

// Check client connections every 10 milliseconds
#define CHECK_CLIENT_INTERVAL 10
// UDP port used for the RTP video stream (and +1 for RTCP)
#define RTP_UDP_PORT 6970
// UDP port used for the RTP audio stream (and +1 for RTCP)
#define AUDIO_UDP_PORT 6972
// Poll the audio source every 20 milliseconds (160 samples at 8 kHz)
#define AUDIO_UPDATE_INTERVAL 20
// Retry a failed RTP fragment after this many milliseconds instead of waiting for the next paced slot
// So a transient Wi-Fi TX failure (errno 12) does not stall the frame for a full fragment interval.
#define FRAGMENT_RETRY_INTERVAL 2

micro_rtsp_server::micro_rtsp_server(micro_rtsp_source_video &video_source, micro_rtsp_source_audio *audio_source /*= nullptr*/)
    : video_source_(video_source),
      audio_source_(audio_source),
      srtp_enabled_(false),
      frame_interval_(200),
      next_frame_update_(0),
      next_audio_update_(0),
      next_check_client_(0),
      rtp_udp_port_(RTP_UDP_PORT),
      audio_udp_port_(AUDIO_UDP_PORT),
      rtsp_port_(554),
      streaming_frame_(false),
      frame_data_start_(nullptr),
      frame_scan_end_(nullptr),
      quant_lum_(nullptr),
      quant_chr_(nullptr),
      frame_timestamp_(0),
      fragment_send_interval_(0),
      next_fragment_send_(0),
      fragment_retry_pending_(false)
{
}

micro_rtsp_server::~micro_rtsp_server()
{
    end();
}

void micro_rtsp_server::begin(unsigned short port /*= 554*/)
{
    rtsp_port_ = port;
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
    rtp_udp_.end();
    audio_udp_.end();
    streaming_frame_ = false;
    clients_.clear();
}

unsigned micro_rtsp_server::get_frame_interval() const
{
    return frame_interval_;
}

unsigned micro_rtsp_server::set_frame_interval(unsigned value)
{
    return frame_interval_ = value;
}

void micro_rtsp_server::set_srtp(bool enabled)
{
    srtp_enabled_ = enabled;
}

size_t micro_rtsp_server::clients() const
{
    return clients_.size();
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
            auto c = std::unique_ptr<rtsp_client>(new rtsp_client(client, video_source_, audio_source_ != nullptr, srtp_enabled_, rtsp_port_));
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

    // Send the next fragment of the current frame, paced across the frame interval so the Wi-Fi TX path is never burst-saturated
    // When a send fails (transient Wi-Fi TX backpressure, e.g. errno 12) the fragment is
    // retried after a short delay instead of waiting for the next paced slot,
    // so a single dropped attempt does not stall the frame.
    if (streaming_frame_ && next_fragment_send_ <= now)
    {
        if (send_next_fragment())
            next_fragment_send_ = now + (fragment_retry_pending_ ? (unsigned long)FRAGMENT_RETRY_INTERVAL : fragment_send_interval_);
        else
            streaming_frame_ = false; // frame fully sent
    }

    // Capture a new frame when the previous one is fully sent and one is due
    if (!streaming_frame_ && next_frame_update_ < now)
    {
        log_v("Stream frame t=%d", next_frame_update_);
        next_frame_update_ = now + frame_interval_;
        start_sending_frame();
    }

    if (audio_source_ != nullptr && next_audio_update_ < now)
    {
        next_audio_update_ = now + AUDIO_UPDATE_INTERVAL;
        send_audio_chunk();
    }
}

bool micro_rtsp_server::send_packet(rtsp_client &client, micro_rtsp_udp &udp, const uint8_t *packet, size_t packet_size, uint16_t dest_port)
{
    if (client.tcp_transport())
    {
        // RTP over RTSP/TCP: the packet already starts with the '$' framing header
        const size_t written = client.write(packet, packet_size);
        if (written != packet_size)
            log_e("TCP interleaved write failed: %u of %u bytes, free_internal: %u", (unsigned)written, (unsigned)packet_size, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA));

        return written == packet_size;
    }
    else
    {
        // RTP over UDP: send the RTP packet (without the 4 byte '$' framing
        // header) in a single sendto() call via the micro_rtsp_udp socket.
        return udp.send(client.remoteIP(), dest_port, packet + rtp_over_tcp_hdr_size, packet_size - rtp_over_tcp_hdr_size);
    }
}

bool micro_rtsp_server::start_sending_frame()
{
    // Nothing to do when no client is actively streaming video
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
        return false;

    // Get the next JPEG frame from the camera. The framebuffer is kept until the whole frame has been transmitted (see send_next_fragment),
    // so the scan/quantization pointers below stay valid across loop() passes.
    video_source_.update_frame();
    if (video_source_.data() == nullptr || video_source_.size() == 0)
        return false;

    const uint8_t *data = video_source_.data();
    const size_t size = video_source_.size();

    // Prepare the frame for streaming. The JPEG header (quantization tables
    // and scan data start) is constant for a fixed quality / frame size, so
    // it is decoded once and cached; subsequent frames skip the full parse.
    if (!jpeg_header_.prepare(data, size))
        return false;

    frame_data_start_ = (uint8_t *)jpeg_header_.scan_start();
    frame_scan_end_ = (uint8_t *)jpeg_header_.scan_end();
    quant_lum_ = jpeg_header_.luminance();
    quant_chr_ = jpeg_header_.chrominance();

    // Reset per-client send progress for the new frame.
    for (auto &client : clients_)
    {
        client->video_frame_offset_ = nullptr;
        client->pending_packet_ = nullptr;
        client->pending_packet_size_ = 0;
    }

    // RTP timestamp for the frame, using a 90 kHz clock (RFC 3551)
    frame_timestamp_ = millis() * 90;

    // Spread the fragments of this frame evenly across the frame interval so
    // the Wi-Fi TX path is never burst-saturated (min 1 ms between fragments).
    const size_t fragment_count = (size_t)(frame_scan_end_ - frame_data_start_) / micro_rtsp_streamer::max_payload_size() + 1;
    fragment_send_interval_ = std::max(1u, frame_interval_ / (unsigned)fragment_count);

    streaming_frame_ = true;
    next_fragment_send_ = millis(); // send the first fragment immediately
    return true;
}

bool micro_rtsp_server::send_next_fragment()
{
    bool frame_done = true;
    fragment_retry_pending_ = false;

    // Each client progresses through the frame independently so a fragment
    // that fails to send (Wi-Fi TX backpressure, e.g. ENOMEM/ENOBUFS) is
    // retried on the next tick instead of being silently dropped.
    for (auto &client : clients_)
    {
        if (!client->active() || !client->video_ready())
            continue;

        // Lazily initialize this client's progress for the current frame.
        if (client->video_frame_offset_ == nullptr)
            client->video_frame_offset_ = frame_data_start_;

        // Build the next fragment if nothing is pending for this client.
        if (client->pending_packet_ == nullptr && client->video_frame_offset_ < frame_scan_end_)
        {
            auto offset = client->video_frame_offset_;
            size_t packet_size = 0;
            auto packet = client->streamer.create_jpg_packet(frame_data_start_, frame_scan_end_, &offset, frame_timestamp_, quant_lum_, quant_chr_, packet_size);
            if (packet == nullptr)
            {
                log_e("Failed to build JPEG RTP fragment");
                return false;
            }

            client->video_frame_offset_ = offset; // advanced by create_jpg_packet
            client->pending_packet_ = packet;     // points into the streamer's fixed buffer
            client->pending_packet_size_ = packet_size;
        }

        // Send the pending fragment; keep it for a retry when the send fails.
        if (client->pending_packet_ != nullptr && send_packet(*client, rtp_udp_, client->pending_packet_, client->pending_packet_size_, client->video_rtp_port()))
        {
            client->pending_packet_ = nullptr;
            client->pending_packet_size_ = 0;
        }

        // A still-pending packet means the send failed this tick (Wi-Fi TX backpressure); request a fast retry instead of the full paced slot.
        if (client->pending_packet_ != nullptr)
            fragment_retry_pending_ = true;

        if (client->pending_packet_ != nullptr || client->video_frame_offset_ < frame_scan_end_)
            frame_done = false;
    }

    return !frame_done;
}

void micro_rtsp_server::send_audio_chunk()
{
    if (audio_source_ == nullptr)
        return;

    auto any_audio = false;
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
            // packet points into the client streamer's fixed buffer; no free needed
            send_packet(*client, audio_udp_, packet, packet_size, client->audio_rtp_port());
        }
    }
}

micro_rtsp_server::rtsp_client::rtsp_client(const WiFiClient &wifi_client, micro_rtsp_source_video &source, bool audio_enabled, bool srtp_enabled, uint16_t rtsp_port)
    : WiFiClient(wifi_client), micro_rtsp_requests(audio_enabled, srtp_enabled), streamer(source), video_frame_offset_(nullptr), pending_packet_(nullptr), pending_packet_size_(0)
{
    // Disable Nagle's algorithm so interleaved RTP/RTCP packets (RTP/AVP/TCP) are sent immediately instead of being coalesced/delayed.
    WiFiClient::setNoDelay(true);

    // Advertise the correct addresses in the SETUP Transport header and the
    // PLAY RTP-Info URL instead of the reference implementation's bogus
    // 127.0.0.1 values: destination = the client, source = this server.
    set_client_ip(std::string(remoteIP().toString().c_str()));
    auto server_ip = WiFi.localIP();
    if (server_ip == IPAddress(0, 0, 0, 0))
        server_ip = WiFi.softAPIP();

    set_server_address(std::string(server_ip.toString().c_str()), rtsp_port);
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
