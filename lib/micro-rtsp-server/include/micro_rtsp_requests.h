#pragma once

#include <string>

class micro_rtsp_requests
{
public:
    std::string process_request(const std::string& request);

private:
    enum rtsp_command
    {
        rtsp_command_unknown,
        rtsp_command_option,   // OPTIONS
        rtsp_command_describe, // DESCRIBE
        rtsp_command_setup,    // SETUP
        rtsp_command_play,     // PLAY
        rtsp_command_teardown  // TEARDOWN
    };

    const std::string available_stream_name_ = "mjpeg/1";

    rtsp_command parse_command(const std::string &request);
    bool parse_client_port(const std::string &request);
    bool parse_cseq(const std::string &request);
    bool parse_stream_url(const std::string &request);

    std::string date_header();
    std::string rtsp_error(unsigned short code, const std::string& message);

    std::string handle_option(const std::string &request);
    std::string handle_describe(const std::string &request);
    std::string handle_setup(const std::string &request);
    std::string handle_play(const std::string &request);
    std::string handle_teardown(const std::string &request);

    unsigned long cseq_;

    std::string host_url_;
    unsigned short host_port_;
    std::string stream_name_;

    bool tcp_transport_;
    unsigned short client_port_;

    unsigned short rtp_streamer_port_;
    unsigned short rtcp_streamer_port_;

    unsigned long rtsp_session_id_;

    bool stream_active_;
    bool stream_stopped_;
};