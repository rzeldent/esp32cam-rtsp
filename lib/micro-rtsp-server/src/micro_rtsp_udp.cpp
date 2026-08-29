// Include our own header first so Arduino's IPAddress.h is processed before the lwIP headers below
// lwIP defines INADDR_NONE/INADDR_ANY macros that would otherwise break IPAddress.h's extern declarations.
#include "micro_rtsp_udp.h"

#include <WiFi.h>
#include <esp32-hal-log.h>
#include <esp_heap_caps.h>

#include <cstring>
#include <errno.h>
#include <lwip/sockets.h>

micro_rtsp_udp::micro_rtsp_udp()
    : sock_(-1)
{
}

micro_rtsp_udp::~micro_rtsp_udp()
{
    end();
}

bool micro_rtsp_udp::begin(uint16_t port)
{
    end(); // close any previously held socket

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        log_e("could not create UDP socket: %d", errno);
        return false;
    }

    sockaddr_in addr = {
        .sin_len = sizeof(addr),
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {
            .s_addr = htonl(INADDR_ANY)}};
    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log_e("could not bind UDP socket to port %u: %d", port, errno);
        ::close(sock);
        return false;
    }

    // Blocking socket: a momentary network-stack squeeze delays the sendto instead of failing it,
    //  which matters less for UDP (lwIP does not queue UDP datagrams) but matches the reference WiFiUDP semantics.
    sock_ = sock;
    return true;
}

void micro_rtsp_udp::end()
{
    if (sock_ >= 0)
    {
        ::close(sock_);
        sock_ = -1;
    }
}

bool micro_rtsp_udp::send(const IPAddress &dest, uint16_t port, const uint8_t *data, size_t len)
{
    if (sock_ < 0)
        return false;

    sockaddr_in dst = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = { 
            .s_addr = (uint32_t)dest }
    };
    const int sent = sendto(sock_, data, len, 0, (sockaddr *)&dst, sizeof(dst));
    if (sent < 0)
    {
        // errno 12 (ENOMEM) maps to lwIP ERR_MEM from the Wi-Fi driver's TX queue being momentarily full when sendto() is called
        // The heap is NOT the cause: the free/largest internal blocks below stay in the tens of KB while streaming
        const size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
        const size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
        log_e("sendto failed: %d, free_internal: %u, largest_internal: %u, RSSI: %d dBm", errno, (unsigned)free_internal, (unsigned)largest_internal, (int)WiFi.RSSI());
        return false;
    }

    return true;
}
