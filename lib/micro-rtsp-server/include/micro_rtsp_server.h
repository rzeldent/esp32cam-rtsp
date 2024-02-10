#pragma once

#include <WiFiServer.h>
#include <micro_rtsp_source.h>
#include <vector>
#include <list>

class micro_rtsp_server : public WiFiServer
{
public:
	micro_rtsp_server(micro_rtsp_source *source, unsigned frame_interval = 100, unsigned short port = 554);
	~micro_rtsp_server();

	void loop();

	unsigned get_frame_interval() { return frame_interval_; }
	unsigned set_frame_interval(unsigned value) { return frame_interval_ = value; }

	size_t clients() { return clients_.size(); }

	class rtsp_client
	{
	public:
		rtsp_client(const WiFiClient &wifi_client);
		void handle_request();

	private:
		enum rtsp_command
		{
			rtsp_command_unknown,
			rtsp_command_option,  // OPTIONS
			rtsp_command_describe, // DESCRIBE
			rtsp_command_setup,	   // SETUP
			rtsp_command_play,	   // PLAY
			rtsp_command_teardown  // TEARDOWN
		};

		rtsp_command parse_command(const String& request);
		unsigned long parse_cseq(const String& request);
		bool parse_stream_url(const String& request);

		int parse_client_port(const String& request);


		WiFiClient wifi_client_;

		bool tcp_transport_;

		String host_url_;
		unsigned short host_port_;
		String stream_name_;
		uint stream_id_;

		unsigned short rtp_port_;
		// enum rtsp_state state_;

		String date_header();

		void handle_option(unsigned long cseq);
		void handle_describe(unsigned long cseq, const String& stream_url);
		void handle_setup(unsigned long cseq, const String& stream_url);
		void handle_play();
		void handle_teardown();
	};

private:
	micro_rtsp_source *source_;

	unsigned frame_interval_;
	unsigned long next_frame_update_;

	unsigned long next_check_client_;
	std::list<rtsp_client *> clients_;
};