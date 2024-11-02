#pragma once

#include <map>
#include <string>

class micro_rtsp_requests
{
public:
    std::string process_request(const std::string& request);

private:
    // enum rtsp_command
    // {
    //     rtsp_command_unknown,
    //     rtsp_command_options,  // OPTIONS
    //     rtsp_command_describe, // DESCRIBE
    //     rtsp_command_setup,    // SETUP
    //     rtsp_command_play,     // PLAY
    //     rtsp_command_teardown  // TEARDOWN
    // };

    static const std::string available_stream_name_;

    //rtsp_command parse_command(const std::string &request);
    //static bool parse_cseq(const std::string &line, unsigned long &cseq);
    bool parse_client_port(const std::string &request);
    //bool parse_stream_url(const std::string &request);

    //static std::string date_header();
    static std::string handle_rtsp_error(unsigned long cseq, unsigned short code, const std::string &message);

    static std::string handle_options(unsigned long cseq);
    static std::string handle_describe(unsigned long cseq, const std::string &request);
    std::string handle_setup(unsigned long cseq, const  std::map<std::string, std::string> &request);
    std::string handle_play(unsigned long cseq);
    std::string handle_teardown(unsigned long cseq);

    //unsigned long cseq_;

    // std::string host_url_;
    // unsigned short host_port_;
    // std::string stream_name_;

    bool tcp_transport_;
    unsigned short start_client_port_;
    unsigned short end_client_port_;

    unsigned short rtp_streamer_port_;
    unsigned short rtcp_streamer_port_;

    unsigned long rtsp_session_id_;

    bool stream_active_;
    bool stream_stopped_;
};