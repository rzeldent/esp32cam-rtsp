#include <esp32-hal-log.h>

#include <iomanip>
#include <unordered_map>
#include <regex>
#include "micro_rtsp_requests.h"

// https://datatracker.ietf.org/doc/html/rfc2326

const std::string micro_rtsp_requests::available_stream_name_ = "/mjpeg/1";

bool micro_rtsp_requests::parse_client_port(const std::string &request)
{
    log_v("request: %s", request.c_str());

    std::regex regex("client_port=([0-9]+)", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_match(request, match, regex))
    {
        log_e("client_port not found");
        return false;
    }

    start_client_port_ = std::stoi(match[1].str());
    return true;
}

std::string micro_rtsp_requests::handle_rtsp_error(unsigned long cseq, unsigned short code, const std::string &message)
{
    log_e("code: %d, message: %s", code, message.c_str());
    auto now = time(nullptr);
    std::ostringstream oss;
    oss << "RTSP/1.0 " << code << " " << message << "\r\n"
        << "CSeq: " << cseq << "\r\n"
        << std::put_time(std::gmtime(&now), "Date: %a, %b %d %Y %H:%M:%S GMT") << "\r\n";
    return oss.str();
}

// OPTIONS rtsp://192.168.178.247:554/mjpeg/1 RTSP/1.0
// CSeq: 2
// User-Agent: LibVLC/3.0.20 (LIVE555 Streaming Media v2016.11.28)
std::string micro_rtsp_requests::handle_options(unsigned long cseq)
{
    auto now = time(nullptr);
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << std::put_time(std::gmtime(&now), "Date: %a, %b %d %Y %H:%M:%S GMT") << "\r\n"
        << "Content-Length: 0\r\n"
        << "Public: DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE";
    return oss.str();
}

// DESCRIBE rtsp://192.168.178.247:554/mjpeg/1 RTSP/1.0
// CSeq: 3
// User-Agent: LibVLC/3.0.20 (LIVE555 Streaming Media v2016.11.28)
// Accept: application/sdp
std::string micro_rtsp_requests::handle_describe(unsigned long cseq, const std::string &request)
{
    // Parse the url
    static const std::regex regex_url("rtsp:\\/\\/([^\\/:]+)(?::(\\d+))?(\\/.*)?\\s+RTSP\\/1\\.0", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_search(request, match, regex_url))
        return handle_rtsp_error(cseq, 400, "Invalid URL");

    auto host = match[1].str();
    auto port = match[2].str().length() > 0 ? std::stoi(match[2].str()) : 554;
    auto path = match[3].str();
    log_i("host: %s, port: %d, path: %s", host.c_str(), port, path.c_str());

    if (path != available_stream_name_)
        return handle_rtsp_error(cseq, 404, "Stream Not Found");

    std::ostringstream osbody;
    osbody << "v=0\r\n"
           << "o=- " << std::rand() << " 1 IN IP4 " << host << "\r\n"
           << "s=\r\n"
           << "t=0 0\r\n"                // start / stop - 0 -> unbounded and permanent session
           << "m=video 0 RTP/AVP 26\r\n" // currently we just handle UDP sessions
           << "c=IN IP4 0.0.0.0\r\n";
    auto body = osbody.str();

    auto now = time(nullptr);
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << std::put_time(std::gmtime(&now), "Date: %a, %b %d %Y %H:%M:%S GMT") << "\r\n"
        << "Content-Base: rtsp://" << host << ":" << port << path << "\r\n"
        << "Content-Type: application/sdp\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

// SETUP rtsp://192.168.178.247:554/mjpeg/1 RTSP/1.0
// CSeq: 0
// Transport: RTP/AVP;unicast;client_port=9058-9059
std::string micro_rtsp_requests::handle_setup(unsigned long cseq, const std::map<std::string, std::string> &lines)
{
    log_v("request: %s", request.c_str());

    auto it = lines.find("Transport");
    if (it == lines.end())
        return handle_rtsp_error(cseq, 400, "No Transport Header Found");

    static const std::regex regex_transport("\\s+RTP\\/AVP(\\/TCP)?;unicast;client_port=(\\d+)-(\\d+)", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_search(it->second, match, regex_transport))
        return handle_rtsp_error(cseq, 400, "Could Not Parse Transport");

    tcp_transport_ = match[1].str().length() > 0;
    start_client_port_ = std::stoi(match[2].str());
    end_client_port_ = std::stoi(match[3].str());
    log_i("tcp_transport: %d, start_client_port: %d, end_client_port: %d", tcp_transport_, start_client_port_, end_client_port_);

    std::ostringstream ostransport;
    if (tcp_transport_)
        ostransport << "RTP/AVP/TCP;unicast;interleaved=0-1";
    else
        ostransport << "RTP/AVP;unicast;destination=127.0.0.1;source=127.0.0.1;client_port=" << start_client_port_ << "-" << end_client_port_ + 1 << ";server_port=" << rtp_streamer_port_ << "-" << rtcp_streamer_port_;

    auto now = time(nullptr);
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << std::put_time(std::gmtime(&now), "Date: %a, %b %d %Y %H:%M:%S GMT") << "\r\n"
        << "Transport: " << ostransport.str() << "\r\n"
        << "Session: " << rtsp_session_id_;
    return oss.str();
}

std::string micro_rtsp_requests::handle_play(unsigned long cseq)
{
    log_v("request: %s", request.c_str());

    stream_active_ = true;

    auto now = time(nullptr);
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << std::put_time(std::gmtime(&now), "Date: %a, %b %d %Y %H:%M:%S GMT") << "\r\n"
        << "Range: npt=0.000-\r\n"
        << "Session: " << rtsp_session_id_ << "\r\n"
        << "RTP-Info: url=rtsp://127.0.0.1:8554" << available_stream_name_ << "/track1" << "\r\n"
         << "\r\n";
    return oss.str();
}

std::string micro_rtsp_requests::handle_teardown(unsigned long cseq)
{
    log_v("request: %s", request.c_str());

    stream_stopped_ = true;

    auto now = time(nullptr);
    std::ostringstream oss;
    oss << "RTSP/1.0 200 OK\r\n"
        << "CSeq: " << cseq << "\r\n"
        << std::put_time(std::gmtime(&now), "Date: %a, %b %d %Y %H:%M:%S GMT") << "\r\n";
    return oss.str();
}

// Parse a request e.g.
// Request: OPTIONS rtsp://192.168.178.247:554/mjpeg/1 RTSP/1.0
// CSeq: 2
// User-Agent: LibVLC/3.0.20 (LIVE555 Streaming Media v2016.11.28)

std::string micro_rtsp_requests::process_request(const std::string &request)
{
    log_v("request: %s", request.c_str());

    std::stringstream ss(request);
    // Get the request line
    std::string request_line;
    if (!std::getline(ss, request_line))
        return handle_rtsp_error(0, 400, "No Request Found");

    // Create a map with headers
    std::string line;
    std::map<std::string, std::string> headers;
    std::size_t pos;
    while (std::getline(ss, line))
    {
        if ((pos = line.find(':')) != std::string::npos)
            headers[line.substr(0, pos)] = line.substr(pos + 1);
        else
            log_e("No : found for header: %s", line.c_str());
    }

    log_i("request_line: %s", request_line.c_str());
    for (const auto &header : headers)
        log_i("header: %s: %s", header.first.c_str(), header.second.c_str());

    // Check for CSeq
    const auto cseq_it = headers.find("CSeq");
    if (cseq_it == headers.end())
        return handle_rtsp_error(0, 400, "No Sequence Found");

    auto cseq = std::stoul(cseq_it->second);

    if (request_line.find("OPTIONS") == 0)
        return handle_options(cseq);
    if (request_line.find("DESCRIBE") == 0)
        return handle_describe(cseq, request_line);
    if (request_line.find("SETUP") == 0)
        return handle_setup(cseq, headers);
    if (request_line.find("PLAY") == 0)
        return handle_play(cseq);
    if (request_line.find("TEARDOWN") == 0)
        return handle_teardown(cseq);

    return handle_rtsp_error(cseq, 400, "Unknown Command or malformed request");
}
