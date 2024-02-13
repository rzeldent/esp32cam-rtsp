#pragma once

#include <Arduino.h>
#include <WiFiServer.h>

#include <string>
#include <list>

#include "micro_rtsp_camera.h"
#include "micro_rtsp_requests.h"

class micro_rtsp_server : WiFiServer
{
public:
	micro_rtsp_server(const micro_rtsp_camera& source, unsigned frame_interval = 100, unsigned short port = 554);
	~micro_rtsp_server();

    unsigned get_frame_interval() { return frame_interval_; }
    unsigned set_frame_interval(unsigned value) { return frame_interval_ = value; }

	void loop();

	size_t clients() { return clients_.size(); }

	class rtsp_client : public WiFiClient, public micro_rtsp_requests
	{
	public:
		rtsp_client(const WiFiClient &client);
		~rtsp_client();

		void handle_request();
	};

private:
	const micro_rtsp_camera &source_;

    unsigned frame_interval_;

	unsigned long next_frame_update_;

	unsigned long next_check_client_;
	std::list<rtsp_client> clients_;
};