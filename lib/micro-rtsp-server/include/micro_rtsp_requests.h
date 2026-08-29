#pragma once

#include <map>
#include <string>
#include <stdint.h>

#include "micro_rtsp_srtp.h"

// Parses and answers RTSP requests (RFC 2326) for a single client session.
// All session state (transport, client ports, SRTP, ...) is kept in this
// object, so one instance is required per connected client.
class micro_rtsp_requests
{
public:
    micro_rtsp_requests(bool audio_enabled = false, bool srtp_enabled = false);

    // Process a complete RTSP request (headers terminated by an empty line,
    // followed by an optional body) and return the response that must be sent
    // back to the client.
    std::string process_request(const std::string &request);

    // State queried by the server to know how and when to stream.
    bool active() const { return stream_active_; }        // PLAY received
    bool stopped() const { return stream_stopped_; }      // TEARDOWN received
    bool tcp_transport() const { return tcp_transport_; } // RTP over RTSP/TCP
    bool video_ready() const { return video_setup_; }     // video track set up
    bool audio_ready() const { return audio_enabled_ && audio_setup_; }
    bool audio_enabled() const { return audio_enabled_; }

    // SRTP: active when the server is configured to use SRTP, or when the
    // client offered "a=crypto" (RFC 4568) in the SETUP request.
    bool srtp_enabled() const { return srtp_enabled_; }
    bool srtp_active() const { return srtp_active_; }
    micro_rtsp_srtp &srtp() { return srtp_; }

    uint16_t video_rtp_port() const { return video_client_rtp_port_; }
    uint16_t video_rtcp_port() const { return video_client_rtcp_port_; }
    uint16_t audio_rtp_port() const { return audio_client_rtp_port_; }
    uint16_t audio_rtcp_port() const { return audio_client_rtcp_port_; }
    unsigned long session_id() const { return rtsp_session_id_; }

    // UDP port numbers of the server, reported in the SETUP reply.
    void set_server_ports(uint16_t rtp, uint16_t rtcp);

    // Addresses reported in the SETUP Transport header (source/destination)
    // and the PLAY RTP-Info URL. Set by the server when the client connects.
    void set_client_ip(const std::string &ip);
    void set_server_address(const std::string &ip, uint16_t rtsp_port);

private:
    static const std::string available_stream_name_;
    static const std::string crypto_suite_; // AES_CM_128_HMAC_SHA1_80

    static std::string handle_rtsp_error(unsigned long cseq, unsigned short code, const std::string &message);
    static std::string handle_options(unsigned long cseq);

    std::string handle_describe(unsigned long cseq, const std::string &request_line);
    std::string handle_setup(unsigned long cseq, const std::string &request_line, const std::map<std::string, std::string> &headers);
    std::string handle_play(unsigned long cseq);
    std::string handle_pause(unsigned long cseq);
    std::string handle_teardown(unsigned long cseq);

    // Parse a Transport header into transport type and client ports.
    bool parse_transport(const std::string &value, bool &tcp, uint16_t &rtp_port, uint16_t &rtcp_port) const;
    // Extract the track number (1 = video, 2 = audio) from the SETUP request line.
    bool parse_track(const std::string &request_line, int &track) const;

    // Build the "a=crypto" attribute (RFC 4568) for the given master key/salt.
    std::string build_crypto_attribute() const;

    // Enable SRTP for this session (generates the master key/salt once).
    void activate_srtp();

    bool audio_enabled_;
    bool srtp_enabled_;   // server configured to always use SRTP
    bool srtp_requested_; // client offered a=crypto in the SETUP body
    bool srtp_active_;    // SRTP in use for this session

    micro_rtsp_srtp srtp_;

    bool tcp_transport_; // video transport (shared by both tracks)
    bool video_setup_;
    bool audio_setup_;
    bool stream_active_;
    bool stream_stopped_;

    uint16_t video_client_rtp_port_;
    uint16_t video_client_rtcp_port_;
    uint16_t audio_client_rtp_port_;
    uint16_t audio_client_rtcp_port_;

    uint16_t rtp_server_port_;
    uint16_t rtcp_server_port_;

    // Client IP (RTP destination) and server IP + RTSP port (RTP source),
    // advertised in the SETUP Transport header and the PLAY RTP-Info URL.
    std::string client_ip_;
    std::string server_ip_;
    uint16_t rtsp_port_;

    unsigned long rtsp_session_id_;
};