#pragma once

#include <IPAddress.h>

#include <stddef.h>
#include <stdint.h>

// Thin RAII wrapper around a raw lwIP UDP socket, used for RTP transport.
// Each datagram is sent with a single sendto() call; the Arduino WiFiUDP class instead copies every byte individually, which is far too slow for a multi-fragment JPEG/RTP stream.
//
// Note: this header includes IPAddress.h before any lwIP header on purpose.
// lwIP's inet.h defines INADDR_NONE/INADDR_ANY as macros, which would break IPAddress.h's `extern IPAddress INADDR_NONE;` if lwIP were included first.
class micro_rtsp_udp
{
public:
    micro_rtsp_udp();
    ~micro_rtsp_udp();

    // Non-copyable: the socket descriptor is owned exclusively.
    micro_rtsp_udp(const micro_rtsp_udp &) = delete;
    micro_rtsp_udp &operator=(const micro_rtsp_udp &) = delete;

    // Create and bind a UDP socket to the given local port.
    // Returns true on success. Closes any previously held socket.
    bool begin(uint16_t port);

    // Close the socket, if open.
    void end();

    // Send one datagram to dest:port. Returns true when sendto() accepted the datagram
    bool send(const IPAddress &dest, uint16_t port, const uint8_t *data, size_t len);

private:
    int sock_;
};
