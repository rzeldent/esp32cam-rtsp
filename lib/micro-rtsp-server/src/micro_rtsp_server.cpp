#include <micro_rtsp_server.h>
#include <vector>
#include <memory>

// Check client connections every 100 milliseconds
#define CHECK_CLIENT_INTERVAL 100

micro_rtsp_server::micro_rtsp_server(micro_rtsp_source *source, unsigned frame_interval /*= 100*/, unsigned short port /*= 554*/)
{
    log_i("starting RTSP server");

    frame_interval_ = frame_interval;
    source_ = source;
    WiFiServer::begin();
}

micro_rtsp_server::~micro_rtsp_server()
{
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
            clients_.push_back(new rtsp_client(client));

        // Check for idle clients
        // clients_.remove_if([](std::unique_ptr<rtsp_client> const &c)
        // 					 { return c->session->m_stopped; });
    }

    if (next_frame_update_ < now)
    {
        next_frame_update_ = now + frame_interval_;

        for (const auto &client : clients_)
        {
            // Handle requests
            client->handle_request();
            // Send the frame. For now return the uptime as time marker, currMs
            // client->session->broadcastCurrentFrame(now);
        }
    }
}

micro_rtsp_server::rtsp_client::rtsp_client(const WiFiClient &wifi_client)
{
    wifi_client_ = wifi_client;
    //    state_ = rtsp_state_unknown;
}

micro_rtsp_server::rtsp_client::rtsp_command micro_rtsp_server::rtsp_client::parse_command(const String &request)
{
    log_i("parse_command");
    // Check for RTSP commands: Option, Describe, Setup, Play, Teardown
    if (request.startsWith("option"))
        return rtsp_command::rtsp_command_option;

    if (request.startsWith("describe"))
        return rtsp_command::rtsp_command_describe;

    if (request.startsWith("setup"))
        return rtsp_command::rtsp_command_setup;

    if (request.startsWith("play"))
        return rtsp_command::rtsp_command_play;

    if (request.startsWith("teardown"))
        return rtsp_command::rtsp_command_teardown;

    return rtsp_command::rtsp_command_unknown;
}

int micro_rtsp_server::rtsp_client::parse_client_port(const String &request)
{
    log_i("parse_client_port");
    auto pos = request.indexOf("client_port=");
    if (pos < 0)
    {
        log_e("client_port not found");
        return 0;
    }

    return strtoul(&request.c_str()[pos + 12], nullptr, 10);
}

unsigned long micro_rtsp_server::rtsp_client::parse_cseq(const String &request)
{
    log_i("parse_cseq");
    auto pos = request.indexOf("cseq: ");
    if (pos < 0)
    {
        log_e("CSeq not found");
        return 0;
    }

    return strtoul(&request.c_str()[pos + 6], nullptr, 10);
}

bool micro_rtsp_server::rtsp_client::parse_stream_url(const String &request)
{
    log_i("parse_host_url");
    // SETUP rtsp://192.168.10.93:1234/mjpeg/1 RTSP/1.0
    auto pos = request.indexOf("rtsp://");
    if (pos < 0)
    {
        log_e("rtsp:// not found");
        return false;
    }

    // Look for :
    auto start = pos + 7;
    pos = request.indexOf(':', start);
    if (pos < 0)
    {
        log_e("parse stream: no host url/post found (: missing)");
        return false;
    }

    // Should be 192.168.10.93
    host_url_ = request.substring(start, pos);
    // Should be 1234
    host_port_ = (unsigned short)(&request.c_str()[pos + 1], nullptr, 10);

    start = pos + 1;
    pos = request.indexOf('/', start);
    if (pos < 0)
    {
        log_e("parse stream: no host port found (/ missing)");
        return false;
    }

    start = pos + 1;
    pos = request.indexOf(' ', start);
    if (pos < 0)
    {
        log_e("parse stream: no stream found (' ' missing)");
        return false;
    }

    // should be mjpeg/1
    stream_name_ = request.substring(start, pos);
    return true;
}

void micro_rtsp_server::rtsp_client::handle_request()
{
    log_i("handle_request");
    // Read if data available
    auto available = wifi_client_.available();
    if (available > 0)
    {
        auto request = wifi_client_.readString();
        // Make response lowercase
        request.toLowerCase();

        auto command = parse_command(request);
        if (command == rtsp_command_unknown)
        {
            log_w("Invalid RTSP command received: %s", request.c_str());
            return;
        }

        unsigned long cseq = parse_cseq(request);
        if (cseq == 0)
        {
            log_w("Invalid sequence: %s", request.c_str());
            return;
        }

        switch (command)
        {
        case rtsp_command_option:
            handle_option(cseq);
            break;
        case rtsp_command_describe:
            handle_describe(cseq, request);
            break;
        case rtsp_command_setup:
            handle_setup(cseq, request);
            break;
        case rtsp_command_play:
            handle_play();
            break;
        case rtsp_command_teardown:
            handle_teardown();
            break;
        default:
            log_e("unknown rtsp_command");
            assert(false);
        }
    }
}

String micro_rtsp_server::rtsp_client::date_header()
{
    char buffer[200];
    auto now = time(nullptr);
    strftime(buffer, sizeof(buffer), "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&now));
    return buffer;
}

void micro_rtsp_server::rtsp_client::handle_option(unsigned long cseq)
{
    log_i("handle_option");
    auto response = String("RTSP/1.0 200 OK\r\n") +
                    String("CSeq: ") + String(cseq) + String("\r\n") +
                    String("Content-Length: 0\r\n") +
                    String("Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n") +
                    String("\r\n");
    wifi_client_.write(response.c_str());
}

void micro_rtsp_server::rtsp_client::handle_describe(unsigned long cseq, const String &request)
{
    log_i("handle_describe");
    if (!parse_stream_url(request))
    {
        log_w("Unable to parse stream url", request.c_str());
        auto response = String("RTSP/1.0 404 Stream Not Found\r\n") +
                        String("CSeq: ") + String(cseq) + String("\r\n") +
                        date_header() + String("\r\n");
        wifi_client_.write(response.c_str());
        return;
    }

    if (stream_name_ != "mjpeg/1")
    {
        log_w("stream %s was requested but is not available", stream_name_.c_str());
        auto response = String("RTSP/1.0 404 Stream Not Found\r\n") +
                        String("CSeq: ") + String(cseq) + String("\r\n") +
                        date_header() +
                        String("\r\n");
        wifi_client_.write(response.c_str());
        return;
    }

    auto sdp = String("v=0\r\n") +
               String("o=- ") + String(host_port_) + String(" 1 IN IP4 ") + String(rand()) + String("\r\n") +
               String("s=\r\n") +
               String("t=0 0\r\n") +                // start / stop - 0 -> unbounded and permanent session
               String("m=video 0 RTP/AVP 26\r\n") + // currently we just handle UDP sessions
               String("c=IN IP4 0.0.0.0\r\n");

    auto response =
        String("RTSP/1.0 200 OK\r\n") +
        String("CSeq: ") + String(cseq) + String("\r\n") +
        date_header() + String("\r\n") +
        String("Content-Base: ") + stream_name_ + ("/\r\n") +
        String("Content-Type: application/sdp\r\n") +
        String("Content-Length: ") + String(sdp.length()) + String("\r\n") +
        String("\r\n") +
        sdp;

    wifi_client_.write(response.c_str());
}

void micro_rtsp_server::rtsp_client::handle_setup(unsigned long cseq, const String &request)
{
            tcp_transport_ = request.indexOf("rtp/avp/tcp") >= 0;

}