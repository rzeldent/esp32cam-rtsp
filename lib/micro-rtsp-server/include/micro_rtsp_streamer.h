#pragma once

#include <micro_rtp_structs.h>

// https://en.wikipedia.org/wiki/Maximum_transmission_unit
constexpr size_t max_wifi_mtu = 2304;
// Payload JPG - https://www.ietf.org/rfc/rfc1890.txt
constexpr uint8_t RTP_PAYLOAD_JPG = 26;
//  http://www.ietf.org/rfc/rfc2345.txt Each table is an array of 64 values given in zig-zag order, identical to the format used in a JFIF DQT marker segment.
constexpr size_t jpeg_luminance_table_length = 64;
constexpr size_t jpeg_chrominance_table_length = 64;

// One of the types below will be returned, the jpeg_packet_with_quantization_t for the first packet, then the jpeg_packet_t

typedef struct __attribute__((packed))
{
    rtp_over_tcp_hdr_t rtp_over_tcp_hdr;
    rtp_hdr_t rtp_hdr;
    jpeg_hdr_t jpeg_hdr;
    jpeg_hdr_qtable_t jpeg_hdr_qtable;
    uint8_t quantization_table_luminance[jpeg_luminance_table_length];
    uint8_t quantization_table_chrominance[jpeg_chrominance_table_length];
    uint8_t jpeg_data[];
} jpeg_packet_with_quantization_t;

typedef struct __attribute__((packed))
{
    rtp_over_tcp_hdr_t rtp_over_tcp_hdr;
    rtp_hdr_t rtp_hdr;
    jpeg_hdr_t jpeg_hdr;
    uint8_t jpeg_data[];
} jpeg_packet_t;

class micro_rtsp_streamer
{
public:
    micro_rtsp_streamer(const uint16_t width, const uint16_t height);
    rtp_over_tcp_hdr_t *create_jpg_packet(const uint8_t *jpg_scan, const uint8_t *jpg_scan_end, uint8_t **jpg_offset, const uint32_t timestamp, const uint8_t *quantization_table_luminance, const uint8_t *quantization_table_chrominance);

private:
    uint16_t width_, height_;
    uint32_t ssrc_;
    uint16_t sequence_number_;
};