#pragma once

#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include <string>
#include <list>
#include <memory>

#include "micro_rtsp_camera.h"
#include "micro_rtsp_requests.h"
#include "micro_rtsp_streamer.h"
#include "micro_rtsp_audio_source.h"
#include "micro_rtsp_srtp.h"

class micro_rtsp_server : WiFiServer
{
public:
	// audio_source is optional. When set, an audio track (G.711 a-law) is
	// offered to the clients and streamed alongside the video.
	micro_rtsp_server(micro_rtsp_source &source, micro_rtsp_audio_source *audio_source = nullptr);
	~micro_rtsp_server();

	void begin(unsigned short port = 554);
	void end();

	unsigned get_frame_interval() const { return frame_interval_; }
	unsigned set_frame_interval(unsigned value) { return frame_interval_ = value; }

	// Enable Secure RTP (RFC 3711) for all clients. The server advertises its
	// SRTP master key/salt through "a=crypto" (RFC 4568) in the DESCRIBE and
	// SETUP replies and encrypts the outgoing RTP streams. A client that
	// offers its own "a=crypto" enables SRTP for that session as well.
	void set_srtp(bool enabled) { srtp_enabled_ = enabled; }

	void loop();

	size_t clients() const { return clients_.size(); }

	class rtsp_client : public WiFiClient, public micro_rtsp_requests
	{
	public:
		rtsp_client(const WiFiClient &client, micro_rtsp_source &source, bool audio_enabled, bool srtp_enabled, uint16_t rtsp_port);
		~rtsp_client();

		void handle_request();

		// Buffers an incoming chunk until a complete request is available.
		void append_request_data(const uint8_t *data, size_t size);

		micro_rtsp_streamer streamer;
		std::string request_buffer;
	};

private:
	// Sends one RTP packet to a client using its configured transport.
	// udp is the UDP socket to use for RTP/UDP transport, dest_port the client
	// RTP port (both ignored for RTP/AVP/TCP interleaved transport).
	void send_packet(rtsp_client &client, WiFiUDP &udp, const uint8_t *packet, size_t packet_size, uint16_t dest_port);
	void send_video_frame();
	void send_audio_chunk();

	micro_rtsp_source &source_;
	micro_rtsp_audio_source *audio_source_;
	bool srtp_enabled_;
	unsigned frame_interval_;
	unsigned long next_frame_update_;
	unsigned long next_audio_update_;
	unsigned long next_check_client_;
	uint16_t rtp_udp_port_;
	uint16_t audio_udp_port_;
	uint16_t rtsp_port_; // RTSP TCP port, advertised in the RTP-Info URL
	WiFiUDP rtp_udp_;
	WiFiUDP audio_udp_;
	std::list<std::unique_ptr<rtsp_client>> clients_;
};