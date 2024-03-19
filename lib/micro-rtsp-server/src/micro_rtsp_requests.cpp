#include <esp32-hal-log.h>

#include <regex>
#include "micro_rtsp_requests.h"

// https://datatracker.ietf.org/doc/html/rfc2326

micro_rtsp_requests::rtsp_command micro_rtsp_requests::parse_command(const std::string &request)
{
    log_i("parse_command");
    // Check for RTSP commands: Option, Describe, Setup, Play, Teardown

    if (std::regex_match(request, std::regex("^OPTION ", std::regex_constants::icase)))
        return rtsp_command::rtsp_command_option;

    if (std::regex_match(request, std::regex("^DESCRIBE ", std::regex_constants::icase)))
        return rtsp_command::rtsp_command_describe;

    if (std::regex_match(request, std::regex("^SETUP ", std::regex_constants::icase)))
        return rtsp_command::rtsp_command_setup;

    if (std::regex_match(request, std::regex("^PLAY ", std::regex_constants::icase)))
        return rtsp_command::rtsp_command_play;

    if (std::regex_match(request, std::regex("^TEARDOWN ", std::regex_constants::icase)))
        return rtsp_command::rtsp_command_teardown;

    log_e("Invalid rtsp command: %s", request.c_str());
    return rtsp_command::rtsp_command_unknown;
}

bool micro_rtsp_requests::parse_client_port(const std::string &request)
{
    log_i("parse_client_port");
    std::regex regex("client_port=([0-9]+)", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_match(request, match, regex))
    {
        log_e("client_port not found");
        return false;
    }

    client_port_ = std::stoi(match[1].str());
    return true;
}

bool micro_rtsp_requests::parse_cseq(const std::string &request)
{
    log_i("parse_cseq");
    // CSeq: 2
    std::regex regex("CSeq: (\\d+)", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_match(request, match, regex))
    {
        log_e("CSeq not found");
        return false;
    }

    cseq_ = std::stoul(match[1].str());
    return true;
}

bool micro_rtsp_requests::parse_stream_url(const std::string &request)
{
    log_i("parse_host_url");
    // SETUP rtsp://192.168.10.93:1234/mjpeg/1 RTSP/1.0
    std::regex regex("rtsp:\\/\\/([\\w.]+):(\\d+)\\/([\\/\\w]+)\\s+RTSP/1.0", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_match(request, match, regex))
    {
        log_e("Unable to parse url");
        return false;
    }

    host_url_ = match[1].str();
    host_port_ = std::stoi(match[2].str());
    stream_name_ = match[3].str();
    return true;
}

std::string micro_rtsp_requests::date_header()
{
    char buffer[50];
    auto now = std::time(nullptr);
    std::strftime(buffer, sizeof(buffer), "Date: %a, %b %d %Y %H:%M:%S GMT", std::gmtime(&now));
    return buffer;
}

std::string micro_rtsp_requests::rtsp_error(unsigned short code, const std::string &message)
{
    std::ostringstream oss;
    oss << "RTSP/1.0 " << code << " " << message << "\r\n"
        << "CSeq: " << cseq_ << "\r\n"
        << date_header() << "\r\n"
        << "\r\n";
    return oss.str();
}

std::string micro_rtsp_requests::handle_option(const std::string &request)
{
    log_i("handle_option");
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq_ << "\r\n"
        << date_header() << "\r\n"
        << "Content-Length: 0\r\n"
        << "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n"
        << "\r\n";
    return oss.str();
}

std::string micro_rtsp_requests::handle_describe(const std::string &request)
{
    log_i("handle_describe");
    if (!parse_stream_url(request))
        return rtsp_error(400, "Invalid Stream Name");

    if (stream_name_ != available_stream_name_)
        return rtsp_error(404, "Stream Not Found");

    std::ostringstream sdpos;
    sdpos << "v=0\r\n"
          << "o=- " << host_port_ << " 1 IN IP4 " << std::rand() << "\r\n"
          << "s=\r\n"
          << "t=0 0\r\n"                // start / stop - 0 -> unbounded and permanent session
          << "m=video 0 RTP/AVP 26\r\n" // currently we just handle UDP sessions
          << "c=IN IP4 0.0.0.0\r\n";
    auto sdp = sdpos.str();

    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq_ << "\r\n"
        << date_header() << "\r\n"
        << "Content-Base: " << stream_name_ << "/\r\n"
        << "Content-Type: application/sdp\r\n"
        << "Content-Length: " << sdp.size() << "\r\n"
        << "\r\n"
        << sdp;
    return oss.str();
}

std::string micro_rtsp_requests::handle_setup(const std::string &request)
{
    tcp_transport_ = request.find("rtp/avp/tcp") != std::string::npos;

    if (!parse_client_port(request))
        return rtsp_error(400, "Could Not Find Client Port");

    std::ostringstream ostransport;
    if (tcp_transport_)
        ostransport << "RTP/AVP/TCP;unicast;interleaved=0-1";
    else
        ostransport << "RTP/AVP;unicast;destination=127.0.0.1;source=127.0.0.1;client_port=" << client_port_ << "-" << client_port_ + 1 << ";server_port=" << rtp_streamer_port_ << "-" << rtcp_streamer_port_;

    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq_ << "\r\n"
        << date_header() << "\r\n"
        << "Transport: " << ostransport.str() << "\r\n"
        << "Session: " << rtsp_session_id_ << "\r\n"
        << "\r\n";
    return oss.str();
}

std::string micro_rtsp_requests::handle_play(const std::string &request)
{
    stream_active_ = true;
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq_ << "\r\n"
        << date_header() << "\r\n"
        << "Range: npt=0.000-\r\n"
        << "Session: " << rtsp_session_id_ << "\r\n"
        << "RTP-Info: url=rtsp://127.0.0.1:8554/" << available_stream_name_ << "/track1\r\n"
        << "\r\n";
    return oss.str();
}

std::string micro_rtsp_requests::handle_teardown(const std::string &request)
{
    stream_stopped_ = true;
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq_ << "\r\n"
        << date_header() << "\r\n"
        << "\r\n";
    return oss.str();
}

std::string micro_rtsp_requests::process_request(const std::string &request)
{
    log_i("handle_request");
    auto command = parse_command(request);
    if (command != rtsp_command_unknown)
    {
        if (!parse_cseq(request))
            return rtsp_error(400, "No Sequence Found");

        switch (command)
        {
        case rtsp_command_option:
            return handle_option(request);
        case rtsp_command_describe:
            return handle_describe(request);
        case rtsp_command_setup:
            return handle_setup(request);
        case rtsp_command_play:
            return handle_play(request);
        case rtsp_command_teardown:
            return handle_teardown(request);
        }
    }

    return rtsp_error(400, "Unknown Command");
}
