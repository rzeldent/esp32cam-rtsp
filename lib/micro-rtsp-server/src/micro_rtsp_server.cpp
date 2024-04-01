#include <micro_rtsp_server.h>
#include <jpg.h>
#include <vector>
#include <memory>

// Check client connections every 100 milliseconds
#define CHECK_CLIENT_INTERVAL 10

micro_rtsp_server::micro_rtsp_server(micro_rtsp_camera &source, unsigned frame_interval /*= 100*/)
    : source_(source), streamer_(source.width(), source.height())
{
    log_v("frame_interval:%d", frame_interval);
    frame_interval_ = frame_interval;
}

micro_rtsp_server::~micro_rtsp_server()
{
    end();
}

void micro_rtsp_server::begin(unsigned short port /*= 554*/)
{
    WiFiServer::begin(port);
}

void micro_rtsp_server::end()
{
    WiFiServer::end();
}

void micro_rtsp_server::loop()
{
    auto now = millis();

    if (next_check_client_ < now)
    {
        log_v("Check for new client");
        next_check_client_ = now + CHECK_CLIENT_INTERVAL;

        // Check if a client wants to connect
        auto client = accept();
        if (client)
            clients_.push_back(rtsp_client(client));

        // Check for idle clients
        clients_.remove_if([](rtsp_client &c)
                           { return !c.connected(); });

        for (auto client : clients_)
            client.handle_request();
    }

    if (next_frame_update_ < now)
    {
        log_v("Stream frame");
        next_frame_update_ = now + frame_interval_;

        auto ts = time(nullptr);
        // Get next jpg frame
        source_.update_frame();
        // Decode to get quantitation- and scan data
        jpg jpg;
        auto jpg_data = source_.data();
        auto jpg_size = source_.size();
        assert(jpg.decode(jpg_data, jpg_size));

        auto jpg_scan_current = (uint8_t *)jpg.jpeg_data_start;
        while (jpg_scan_current < jpg.jpeg_data_end)
        {
            auto packet = streamer_.create_jpg_packet(jpg.jpeg_data_start, jpg.jpeg_data_end, &jpg_scan_current, ts, jpg.quantization_table_luminance_->data, jpg.quantization_table_chrominance_->data);
            for (auto client : clients_)
            {
                log_v("Stream frame to client: 0x%08x", client);
                // RTP over TCP encapsulates in a $
                client.write((const uint8_t *)packet, packet->length + sizeof(micro_rtsp_streamer::rtp_over_tcp_hdr_t));
                // TODO: UDP
            }
            free(packet);
        }
    }
}

micro_rtsp_server::rtsp_client::rtsp_client(const WiFiClient &wifi_client)
    : WiFiClient(wifi_client)
{
}

micro_rtsp_server::rtsp_client::~rtsp_client()
{
    stop();
}

void micro_rtsp_server::rtsp_client::handle_request()
{
    // Read if data available
    auto bytes_available = available();
    if (bytes_available > 0)
    {
        std::string request;
        request.reserve(bytes_available);
        if (read((uint8_t *)request.data(), bytes_available) == bytes_available)
        {
            log_i("Request: %s", request);
            auto response = process_request(request);
            log_i("Response: %s", response);
            write(response.data(), response.size());
        }
    }
}
