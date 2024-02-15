#include <micro_rtsp_server.h>
#include <vector>
#include <memory>

// Check client connections every 100 milliseconds
#define CHECK_CLIENT_INTERVAL 10

micro_rtsp_server::micro_rtsp_server(const micro_rtsp_camera &source, unsigned frame_interval /*= 100*/)
    : source_(source)
{
    log_i("starting RTSP server");
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
        next_frame_update_ = now + frame_interval_;
        for (auto client : clients_)
        {
            ;
            // client->session->broadcastCurrentFrame(now);
        }
    }
}

micro_rtsp_server::rtsp_client::rtsp_client(const WiFiClient &wifi_client)
    : WiFiClient(wifi_client)
{
    log_i("rtsp_client");
}

micro_rtsp_server::rtsp_client::~rtsp_client()
{
    log_i("~rtsp_client");
    stop();
}

void micro_rtsp_server::rtsp_client::handle_request()
{
    log_i("handle_request");
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
