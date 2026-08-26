#pragma once

#include <stddef.h>
#include <stdint.h>

// RTP over RTSP/TCP framing header (RFC 2326 section 10.12). This 4 byte
// header is prepended to every RTP packet when the transport is "RTP/AVP/TCP"
// (interleaved). The length field is the number of bytes of the encapsulated
// RTP packet, in network byte order.
typedef struct __attribute__((packed))
{
    uint8_t magic;   // always '$' (0x24)
    uint8_t channel; // channel identifier (0/1 = video, 2/3 = audio)
    uint16_t length; // length of the encapsulated RTP packet (network order)
} rtp_over_tcp_hdr_t;

// Fixed header sizes, in bytes.
constexpr size_t rtp_over_tcp_hdr_size = sizeof(rtp_over_tcp_hdr_t); // 4
constexpr size_t rtp_hdr_size = 12;                                  // RFC 3550
constexpr size_t jpeg_hdr_size = 8;                                  // RFC 2435
constexpr size_t jpeg_qtable_hdr_size = 4; // MBZ + precision + length
constexpr size_t jpeg_qtable_size = 64;    // bytes in one quantization table

// Maximum number of JPEG scan bytes carried in a single RTP packet. Kept well
// below the 1500 byte Ethernet MTU so RTP/UDP datagrams are never fragmented.
constexpr size_t max_jpeg_payload_size = 1100;

// RTP payload types used by this server (RFC 3551).
constexpr uint8_t RTP_PAYLOAD_JPG = 26; // JPEG (RFC 2435)
constexpr uint8_t RTP_PAYLOAD_PCMA = 8; // G.711 a-law, 8 kHz
