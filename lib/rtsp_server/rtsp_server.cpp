#include "rtsp_server.h"
#include <esp32-hal-log.h>
#include <OV2640Streamer.h>

// URI: e.g. rtsp://192.168.178.27:554/mjpeg/1
rtsp_server::rtsp_server(OV2640 &cam, unsigned long interval, int port /*= 554*/)
	: WiFiServer(port), cam_(cam)
{
	log_i("Starting RTSP server");
	WiFiServer::begin();
	timer_.every(interval, client_handler, this);
}
size_t rtsp_server::num_connected()
{
	return clients_.size();
}

void rtsp_server::doLoop()
{
	timer_.tick();
}

rtsp_server::rtsp_client::rtsp_client(const WiFiClient &client, OV2640 &cam)
{
	wifi_client = client;
	streamer = std::shared_ptr<OV2640Streamer>(new OV2640Streamer(&wifi_client, cam));
	session = std::shared_ptr<CRtspSession>(new CRtspSession(&wifi_client, streamer.get()));
}

bool rtsp_server::client_handler(void *arg)
{
	auto self = static_cast<rtsp_server *>(arg);
	// Check if a client wants to connect
	auto new_client = self->accept();
	if (new_client)
		self->clients_.push_back(std::unique_ptr<rtsp_client>(new rtsp_client(new_client, self->cam_)));

	auto now = millis();
	for (const auto &client : self->clients_)
	{
		// Handle requests
		client->session->handleRequests(0);
		// Send the frame. For now return the uptime as time marker, currMs
		client->session->broadcastCurrentFrame(now);
	}

	self->clients_.remove_if([](std::unique_ptr<rtsp_client> const &c)
							 { return c->session->m_stopped; });

	return true;
}