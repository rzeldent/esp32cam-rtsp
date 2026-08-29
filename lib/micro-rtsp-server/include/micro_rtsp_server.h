#pragma once

#include <Arduino.h>
#include <WiFiServer.h>

#include <string>
#include <list>
#include <memory>

#include "micro_rtsp_source_audio.h"
#include "micro_rtsp_source_video.h"
#include <jpeg_header.h>
#include "micro_rtsp_requests.h"
#include "micro_rtsp_srtp.h"
#include "micro_rtsp_streamer.h"
#include "micro_rtsp_udp.h"

class micro_rtsp_server : WiFiServer
{
public:
	// Both sources are optional. When a source is set and available, the matching track
	// (video: MJPEG, audio: G.711 a-law) is offered to clients and streamed. A device
	// without a camera or microphone serves only the tracks it actually has.
	micro_rtsp_server( micro_rtsp_source_video *video_source = nullptr, micro_rtsp_source_audio *audio_source = nullptr);
	~micro_rtsp_server();

	void begin();
	void end();

	unsigned short get_rtsp_port() const { return rtsp_port_; }
	unsigned short set_rtsp_port(unsigned short port) { return rtsp_port_ = port; }
	unsigned get_frame_interval() const { return frame_interval_; }
	unsigned set_frame_interval(unsigned value) { return frame_interval_ = value; }

	// Enable Secure RTP (RFC 3711) for all clients. The server advertises its SRTP master key/salt through "a=crypto" (RFC 4568) in the DESCRIBE and SETUP replies and encrypts the outgoing RTP streams
	// A client that offers its own "a=crypto" enables SRTP for that session as well.
	void set_srtp(bool enabled) { srtp_enabled_ = enabled; }

	// Enable RTSP Basic authentication (RFC 2617) for all clients.
	// An empty username disables authentication.
	void set_credentials(const std::string &username, const std::string &password) { username_ = username; password_ = password; }

	void loop();

	size_t clients() const { return clients_.size(); }

	class rtsp_client : public WiFiClient, public micro_rtsp_requests
	{
	public:
		rtsp_client(const WiFiClient &client, bool video_enabled, bool audio_enabled, bool srtp_enabled, uint16_t rtsp_port);
		~rtsp_client();

		void handle_request();

		// Returns true when the RTSP control connection has been closed by the peer
		// (clean FIN or reset). WiFiClient::connected() can stay true after a clean
		// FIN (socket in CLOSE_WAIT), so this probes the socket for EOF.
		bool peer_closed();

		// Buffers an incoming chunk until a complete request is available.
		void append_request_data(const uint8_t *data, size_t size);

		micro_rtsp_streamer streamer;
		std::string request_buffer;

		// Per-frame send progress: the next fragment offset within the current frame.
		uint8_t *video_frame_offset_;
	};

private:
	// Sends one RTP packet to a client using its configured transport.
	// UDP is the socket used for RTP/UDP transport, dest_port the client RTP port (both ignored for RTP/AVP/TCP interleaved transport).
	// Returns false when the packet could not be sent and should be retried.
	bool send_packet(rtsp_client &client, micro_rtsp_udp &udp, const uint8_t *packet, size_t packet_size, uint16_t dest_port);
	bool start_sending_frame(); // capture + decode a frame, begin paced transmission
	bool send_next_fragment();	// send one fragment, false when the frame is done
	void send_audio_chunk();

	micro_rtsp_source_video *video_source_;
	micro_rtsp_source_audio *audio_source_;
	bool srtp_enabled_;

	// RTSP Basic auth credentials (RFC 2617); empty username disables auth.
	std::string username_;
	std::string password_;

	unsigned frame_interval_;
	unsigned long next_frame_update_;
	unsigned long next_audio_update_;
	unsigned long next_check_client_;
	uint16_t rtp_udp_port_;
	uint16_t audio_udp_port_;
	uint16_t rtsp_port_ = 554;	// 554 is the default RTSP port, see RFC 2326. The server listens on this TCP port for incoming RTSP connections and advertises it in the RTP-Info URL.
	micro_rtsp_udp rtp_udp_;   // UDP socket for the RTP video stream
	micro_rtsp_udp audio_udp_; // UDP socket for the RTP audio stream
	std::list<std::unique_ptr<rtsp_client>> clients_;

	// Paced transmission of the current frame: the captured framebuffer is held until every fragment has been sent, and the fragments are spread
	// evenly across the frame interval so the Wi-Fi TX path is never burst-saturated.
	bool streaming_frame_;
	uint8_t *frame_data_start_; // start of the scan data (first fragment)
	uint8_t *frame_scan_end_;	// end of the scan data
	const uint8_t *quant_lum_;	// quantization tables (valid while streaming)
	const uint8_t *quant_chr_;
	uint32_t frame_timestamp_;		   // RTP timestamp (90 kHz) for this frame
	unsigned fragment_send_interval_;  // ms between two fragments
	unsigned long next_fragment_send_; // when to send the next fragment

	// Cached JPEG header (quantization tables + scan data start offset),
	// reused across frames so the full JPEG parse only happens once.
	jpeg_header jpeg_header_;
};