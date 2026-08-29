#include <esp32-hal-log.h>
#include <esp_random.h>

#include <sstream>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <regex>
#include "micro_rtsp_requests.h"

// https://datatracker.ietf.org/doc/html/rfc2326
// https://datatracker.ietf.org/doc/html/rfc4568 (crypto attributes)

namespace
{
    // Minimal base64 encoder (RFC 4648), used to carry the SRTP master key and salt in the "a=crypto" attribute.
    std::string base64_encode(const uint8_t *data, size_t len)
    {
        static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);
        for (size_t i = 0; i < len; i += 3)
        {
            uint32_t n = (uint32_t)data[i] << 16;
            if (i + 1 < len)
                n |= (uint32_t)data[i + 1] << 8;
            if (i + 2 < len)
                n |= data[i + 2];

            out += table[(n >> 18) & 0x3f];
            out += table[(n >> 12) & 0x3f];
            out += (i + 1 < len) ? table[(n >> 6) & 0x3f] : '=';
            out += (i + 2 < len) ? table[n & 0x3f] : '=';
        }
        return out;
    }

    // Minimal base64 decoder (RFC 4648), used to verify the RTSP Basic
    // authorization header. Returns an empty string on invalid input.
    std::string base64_decode(const std::string &in)
    {
        static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        int buffer = 0;
        int bits = 0;
        for (char c : in)
        {
            if (c == '=' || c == '\r' || c == '\n' || c == ' ')
                continue;
            const char *p = strchr(table, c);
            if (p == nullptr)
                return {};
            buffer = (buffer << 6) | (int)(p - table);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out.push_back((char)((buffer >> bits) & 0xff));
            }
        }
        return out;
    }

    // Build the common RTSP response head: status line, CSeq and Date header
    // (RFC 2326 12.2). Uses std::to_string for the numeric fields instead of
    // an ostringstream.
    std::string rtsp_response_header(unsigned long cseq, unsigned short code, const std::string &reason)
    {
        auto now = time(nullptr);
        char date[64];
        std::strftime(date, sizeof(date), "%a, %b %d %Y %H:%M:%S GMT", std::gmtime(&now));
        return "RTSP/1.0 " + std::to_string(code) + " " + reason + "\r\n" + "CSeq: " + std::to_string(cseq) + "\r\n" + "Date: " + date + "\r\n";
    }

    // Trim leading/trailing whitespace (including CR/LF) from a string.
    std::string trimmed(const std::string &s)
    {
        auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};
        auto last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    }

    // Extract the method token from an RTSP request line,
    // e.g. "DESCRIBE rtsp://... RTSP/1.0" -> "DESCRIBE".
    std::string request_method(const std::string &request_line)
    {
        auto space = request_line.find(' ');
        return request_line.substr(0, space);
    }
} // namespace

const std::string micro_rtsp_requests::available_stream_name_ = "/mjpeg/1";
const std::string micro_rtsp_requests::crypto_suite_ = "AES_CM_128_HMAC_SHA1_80";

micro_rtsp_requests::micro_rtsp_requests(bool video_enabled /*= true*/, bool audio_enabled /*= false*/, bool srtp_enabled /*= false*/)
    : video_enabled_(video_enabled),
      audio_enabled_(audio_enabled),
      srtp_enabled_(srtp_enabled),
      srtp_requested_(false),
      srtp_active_(false),
      tcp_transport_(false),
      video_setup_(false),
      audio_setup_(false),
      stream_active_(false),
      stream_stopped_(false),
      video_client_rtp_port_(0),
      video_client_rtcp_port_(0),
      audio_client_rtp_port_(0),
      audio_client_rtcp_port_(0),
      rtp_server_port_(0),
      rtcp_server_port_(0),
      client_ip_(),
      server_ip_(),
      rtsp_port_(554) // default RTSP port, replaced by set_server_address()
{
    // Create a unique session id for this client
    rtsp_session_id_ = esp_random() | 0x80000000;
}

void micro_rtsp_requests::set_server_ports(uint16_t rtp_server_port, uint16_t rtcp_server_port)
{
    rtp_server_port_ = rtp_server_port;
    rtcp_server_port_ = rtcp_server_port;
}

void micro_rtsp_requests::set_client_ip(const std::string &client_ip)
{
    client_ip_ = client_ip;
}

void micro_rtsp_requests::set_server_address(const std::string &server_ip, uint16_t rtsp_port)
{
    server_ip_ = server_ip;
    rtsp_port_ = rtsp_port;
}

void micro_rtsp_requests::set_credentials(const std::string &username, const std::string &password)
{
    username_ = username;
    password_ = password;
}

// Verifies the "Authorization: Basic <base64(user:pass)>" header (RFC 2617)
// against the configured credentials.
bool micro_rtsp_requests::check_authorization(const std::map<std::string, std::string> &headers) const
{
    auto it = headers.find("Authorization");
    if (it == headers.end())
        return false;

    const auto &value = it->second;
    if (value.compare(0, 6, "Basic ") != 0)
        return false;

    auto expected = username_ + ":" + password_;
    return base64_decode(value.substr(6)) == expected;
}

std::string micro_rtsp_requests::handle_unauthorized(unsigned long cseq)
{
    return rtsp_response_header(cseq, 401, "Unauthorized") + "WWW-Authenticate: Basic realm=\"ESP32CAM-RTSP\"\r\n" + "\r\n";
}

bool micro_rtsp_requests::parse_transport(const std::string &value, bool &tcp, uint16_t &rtp_port, uint16_t &rtcp_port) 
{
    // RTP/AVP/TCP;unicast;interleaved=0-1
    static const std::regex regex_tcp("RTP\\/AVP\\/TCP", std::regex_constants::icase);
    static const std::regex regex_ports("client_port=(\\d+)-(\\d+)", std::regex_constants::icase);
    std::smatch match;
    tcp = std::regex_search(value, match, regex_tcp);
    if (tcp)
        return true; // interleaved channels are used, no client ports

    if (!std::regex_search(value, match, regex_ports))
        return false;

    rtp_port = (uint16_t)std::stoul(match[1].str());
    rtcp_port = (uint16_t)std::stoul(match[2].str());
    return true;
}

bool micro_rtsp_requests::parse_track(const std::string &request_line, int &track)
{
    static const std::regex regex_track("(?:\\/track|trackID=)(\\d+)", std::regex_constants::icase);
    std::smatch match;
    if (std::regex_search(request_line, match, regex_track))
    {
        track = std::stoi(match[1].str());
        return true;
    }

    // No track specified: default to the video track
    track = 1;
    return true;
}

// Build the RFC 4568 crypto attribute from the SRTP master key and salt:
//   a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:<base64(key || salt)>
std::string micro_rtsp_requests::build_crypto_attribute() const
{
    uint8_t key_salt[micro_rtsp_srtp::master_key_size + micro_rtsp_srtp::master_salt_size];
    memcpy(key_salt, srtp_.key(), micro_rtsp_srtp::master_key_size);
    memcpy(key_salt + micro_rtsp_srtp::master_key_size, srtp_.salt(), micro_rtsp_srtp::master_salt_size);
    auto b64 = base64_encode(key_salt, sizeof(key_salt));
    auto attribute = "a=crypto:1 " + crypto_suite_ + " inline:" + b64;
    log_v("crypto attribute: %s", attribute.c_str());
    return attribute;
}

void micro_rtsp_requests::activate_srtp()
{
    if (!srtp_active_)
    {
        // The server generates its own master key/salt and advertises it in the DESCRIBE/SETUP replies; the client uses it to decrypt.
        srtp_.generate_key_salt();
        srtp_active_ = true;
        log_i("SRTP enabled for session");
    }
}

std::string micro_rtsp_requests::handle_rtsp_error(unsigned long cseq, unsigned short code, const std::string &message)
{
    log_e("code: %d, message: %s", code, message.c_str());
    return rtsp_response_header(cseq, code, message) + "\r\n";
}

// OPTIONS rtsp://192.168.178.247:554/mjpeg/1 RTSP/1.0
// CSeq: 2
// User-Agent: LibVLC/3.0.20 (LIVE555 Streaming Media v2016.11.28)
std::string micro_rtsp_requests::handle_options(unsigned long cseq)
{
    return rtsp_response_header(cseq, 200, "OK") +
           "Content-Length: 0\r\n"
           "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE\r\n"
           "\r\n";
}

// DESCRIBE rtsp://192.168.178.247:554/mjpeg/1 RTSP/1.0
// CSeq: 3
// User-Agent: LibVLC/3.0.20 (LIVE555 Streaming Media v2016.11.28)
// Accept: application/sdp
std::string micro_rtsp_requests::handle_describe(unsigned long cseq, const std::string &request_line)
{
    // Parse the url
    static const std::regex regex_url("rtsp:\\/\\/([^\\/:]+)(?::(\\d+))?(\\/.*)?\\s+RTSP\\/1\\.0", std::regex_constants::icase);
    std::smatch match;
    if (!std::regex_search(request_line, match, regex_url))
        return handle_rtsp_error(cseq, 400, "Invalid URL");

    auto host = match[1].str();
    auto port = match[2].str().length() > 0 ? std::stoi(match[2].str()) : rtsp_port_;
    auto path = match[3].str();
    log_i("host: %s, port: %d, path: %s", host.c_str(), port, path.c_str());

    if (path != available_stream_name_ && path != available_stream_name_ + "/")
        return handle_rtsp_error(cseq, 404, "Stream Not Found");

    std::string body = "v=0\r\n"
                       "o=- " + std::to_string(std::rand()) + " 1 IN IP4 " + host + "\r\n"
                       "s=\r\n"
                       "t=0 0\r\n"; // start / stop - 0 -> unbounded and permanent session
    if (video_enabled_)
        body += "m=video 0 RTP/AVP 26\r\n" // JPEG video track
                "c=IN IP4 0.0.0.0\r\n" 
                "a=control:track1\r\n";
    if (srtp_enabled_)
    {
        activate_srtp();
        body += build_crypto_attribute() + "\r\n";
    }
    if (audio_enabled_)
        body += "m=audio 0 RTP/AVP 8\r\n" // G.711 a-law audio track
                "c=IN IP4 0.0.0.0\r\n" 
                "a=rtpmap:8 PCMA/8000/1\r\n" 
                "a=control:track2\r\n";

    return rtsp_response_header(cseq, 200, "OK") + 
        "Content-Base: rtsp://" + host + ":" + std::to_string(port) + path + "/\r\n" 
        "Content-Type: application/sdp\r\n" 
        "Content-Length: " + std::to_string(body.size()) + "\r\n" 
        "\r\n" + body;
}

// SETUP rtsp://192.168.178.247:554/mjpeg/1/track1 RTSP/1.0
// CSeq: 4
// Transport: RTP/AVP;unicast;client_port=9058-9059
std::string micro_rtsp_requests::handle_setup(unsigned long cseq, const std::string &request_line, const std::map<std::string, std::string> &headers)
{
    int track = 1;
    parse_track(request_line, track);
    log_i("track: %d", track);

    if (track == 1 && !video_enabled_)
        return handle_rtsp_error(cseq, 404, "Stream Not Found");

    if (track == 2 && !audio_enabled_)
        return handle_rtsp_error(cseq, 404, "Stream Not Found");

    auto it = headers.find("Transport");
    if (it == headers.end())
        return handle_rtsp_error(cseq, 400, "No Transport Header Found");

    bool tcp = false;
    uint16_t rtp_port = 0;
    uint16_t rtcp_port = 0;
    if (!parse_transport(it->second, tcp, rtp_port, rtcp_port))
        return handle_rtsp_error(cseq, 400, "Could Not Parse Transport");

    tcp_transport_ = tcp;
    if (track == 2)
        audio_setup_ = true;
    else
        video_setup_ = true;

    std::string transport;
    if (tcp)
    {
        // Both tracks are multiplexed over the RTSP TCP connection using
        // interleaved channels (0/1 = video, 2/3 = audio).
        const int channel = (track == 2) ? 2 : 0;
        transport = "RTP/AVP/TCP;unicast;interleaved=" + std::to_string(channel) + "-" + std::to_string(channel + 1);
    }
    else
    {
        if (track == 2)
        {
            audio_client_rtp_port_ = rtp_port;
            audio_client_rtcp_port_ = rtcp_port;
        }
        else
        {
            video_client_rtp_port_ = rtp_port;
            video_client_rtcp_port_ = rtcp_port;
        }
        transport = "RTP/AVP;unicast;destination=" + client_ip_ + ";source=" + server_ip_ + ";client_port=" + std::to_string(rtp_port) + "-" + std::to_string(rtcp_port) + ";server_port=" + std::to_string(rtp_server_port_) + "-" + std::to_string(rtcp_server_port_);
    }

    log_i("tcp_transport: %d, rtp_port: %d, rtcp_port: %d", tcp, rtp_port, rtcp_port);

    std::string response = rtsp_response_header(cseq, 200, "OK") +
        "Transport: " + transport + "\r\n"
        "Session: " + std::to_string(rtsp_session_id_) + "\r\n";

    // When SRTP is used (server configured, or the client offered a=crypto),
    // answer with our own crypto attribute in the message body (RFC 4568).
    if (srtp_enabled_ || srtp_requested_)
    {
        activate_srtp();
        auto crypto = build_crypto_attribute() + "\r\n"
            "Content-Type: application/sdp\r\n";
        response += "Content-Length: " + std::to_string(crypto.size()) + "\r\n" 
            "\r\n" + crypto;
    }
    else
        response += "\r\n";

    return response;
}

std::string micro_rtsp_requests::handle_play(unsigned long cseq)
{
    if (!video_setup_)
        return handle_rtsp_error(cseq, 455, "Method Not Valid In This State");

    stream_active_ = true;

    std::string response = rtsp_response_header(cseq, 200, "OK") +
        "Range: npt=0.000-\r\n"
        "Session: " + std::to_string(rtsp_session_id_) + "\r\n"
        "RTP-Info: url=rtsp://" + server_ip_ + ":" + std::to_string(rtsp_port_) + available_stream_name_ + "/track1";
    if (audio_setup_)
        response += ",url=rtsp://" + server_ip_ + ":" + std::to_string(rtsp_port_) + available_stream_name_ + "/track2";
    
    response += "\r\n\r\n";
    return response;
}

std::string micro_rtsp_requests::handle_pause(unsigned long cseq)
{
    stream_active_ = false;
    return rtsp_response_header(cseq, 200, "OK") + 
        "Session: " + std::to_string(rtsp_session_id_) + "\r\n" 
         "\r\n";
}

std::string micro_rtsp_requests::handle_teardown(unsigned long cseq)
{
    // Stop streaming immediately (not just at the next idle-client sweep):
    // send_next_fragment()/send_audio_chunk() skip clients where active() is
    // false, so no further RTP is sent to this client once TEARDOWN is handled.
    stream_active_ = false;
    stream_stopped_ = true;
    return rtsp_response_header(cseq, 200, "OK") + "\r\n";
}

// Parse a request e.g.
// Request: OPTIONS rtsp://192.168.178.247:554/mjpeg/1 RTSP/1.0
// CSeq: 2
// User-Agent: LibVLC/3.0.20 (LIVE555 Streaming Media v2016.11.28)
std::string micro_rtsp_requests::process_request(const std::string &request)
{
    log_v("request: %s", request.c_str());

    std::istringstream ss(request);

    // Request line
    std::string request_line;
    if (!std::getline(ss, request_line))
        return handle_rtsp_error(0, 400, "No Request Found");
    request_line = trimmed(request_line);

    // Headers (key: value), then an optional body after an empty line
    std::map<std::string, std::string> headers;
    std::string body;
    bool in_body = false;
    std::string line;
    while (std::getline(ss, line))
    {
        line = trimmed(line);
        if (line.empty())
        {
            in_body = true; // end of the header section
            continue;
        }

        if (in_body)
        {
            body += line + "\r\n";
            continue;
        }

        auto colon = line.find(':');
        if (colon != std::string::npos)
            headers[trimmed(line.substr(0, colon))] = trimmed(line.substr(colon + 1));
    }

    // RFC 4568: a client may offer SRTP with an "a=crypto" attribute in the
    // SETUP message body. When it does, the server enables SRTP for the session.
    if (body.find("a=crypto:") != std::string::npos)
    {
        log_i("Client offered SRTP (a=crypto)");
        srtp_requested_ = true;
    }

    log_i("request_line: %s", request_line.c_str());
    for (const auto &header : headers)
        log_i("header: %s: %s", header.first.c_str(), header.second.c_str());
    if (!body.empty())
        log_i("body: %s", body.c_str());

    // CSeq is required for every request (RFC 2326 12.17)
    const auto cseq_it = headers.find("CSeq");
    if (cseq_it == headers.end())
        return handle_rtsp_error(0, 400, "No Sequence Found");
    auto cseq = std::stoul(cseq_it->second);

    // RTSP Basic authentication (RFC 2617). OPTIONS stays open so clients can
    // discover the endpoint; every other request must present credentials when
    // a username is configured.
    const auto method = request_method(request_line);
    if (!username_.empty() && method != "OPTIONS" && !check_authorization(headers))
        return handle_unauthorized(cseq);

    if (method == "OPTIONS")
        return handle_options(cseq);
    if (method == "DESCRIBE")
        return handle_describe(cseq, request_line);
    if (method == "SETUP")
        return handle_setup(cseq, request_line, headers);
    if (method == "PLAY")
        return handle_play(cseq);
    if (method == "PAUSE")
        return handle_pause(cseq);
    if (method == "TEARDOWN")
        return handle_teardown(cseq);

    return handle_rtsp_error(cseq, 400, "Unknown Command or malformed request");
}
