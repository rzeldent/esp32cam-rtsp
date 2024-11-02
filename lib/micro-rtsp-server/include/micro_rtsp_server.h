#pragma once

#include <Arduino.h>
#include <WiFiServer.h>

#include <string>
#include <list>

#include "micro_rtsp_camera.h"
#include "micro_rtsp_requests.h"
#include "micro_rtsp_streamer.h"

class micro_rtsp_server : WiFiServer
{
public:
	micro_rtsp_server(micro_rtsp_source &source);
	~micro_rtsp_server();

	void begin(unsigned short port = 554);
	void end();

	unsigned get_frame_interval() const { return frame_interval_; }
	unsigned set_frame_interval(unsigned value) { return frame_interval_ = value; }

	void loop();

	size_t clients() const { return clients_.size(); }

	class rtsp_client : public WiFiClient, public micro_rtsp_requests
	{
	public:
		rtsp_client(const WiFiClient &client);
		~rtsp_client();

		void handle_request();
	};

private:
	micro_rtsp_source &source_;
	unsigned frame_interval_;
	unsigned long next_frame_update_;
	unsigned long next_check_client_;
	micro_rtsp_streamer streamer_;
	std::list<rtsp_client> clients_;
};