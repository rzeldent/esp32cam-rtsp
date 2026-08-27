// Include our own header first so Arduino's IPAddress.h is processed before
// the lwIP headers below (lwIP defines INADDR_NONE/INADDR_ANY macros that
// would otherwise break IPAddress.h's extern declarations).
#include "micro_rtsp_udp.h"

#include <esp32-hal-log.h>

#include <cstring>
#include <errno.h>
#include <fcntl.h>
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

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log_e("could not bind UDP socket to port %u: %d", port, errno);
        ::close(sock);
        return false;
    }

    // Non-blocking so send() never blocks the calling task
    fcntl(sock, F_SETFL, O_NONBLOCK);
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

    sockaddr_in dst;
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = (uint32_t)dest;
    dst.sin_port = htons(port);
    const int sent = sendto(sock_, data, len, 0, (sockaddr *)&dst, sizeof(dst));
    if (sent < 0)
    {
        log_e("sendto failed: %d", errno);
        return false;
    }
    return true;
}