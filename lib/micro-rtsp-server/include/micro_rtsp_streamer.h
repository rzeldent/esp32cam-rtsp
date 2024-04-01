#pragma once

// https://en.wikipedia.org/wiki/Maximum_transmission_unit
constexpr size_t max_wifi_mtu = 2304;
// Payload JPG - https://www.ietf.org/rfc/rfc1890.txt
constexpr uint8_t RTP_PAYLOAD_JPG = 26;
//  http://www.ietf.org/rfc/rfc2345.txt Each table is an array of 64 values given in zig-zag order, identical to the format used in a JFIF DQT marker segment.
constexpr size_t jpeg_luminance_table_length = 64;
constexpr size_t jpeg_chrominance_table_length = 64;

// https://www.ietf.org/rfc/rfc2326#section-10.12
typedef struct __attribute__((packed))
{
    char magic;      // Magic encapsulation ASCII dollar sign (24 hexadecimal)
    uint8_t channel; // Channel identifier
    uint16_t length; // Network order
} rtp_over_tcp_hdr_t;

class micro_rtsp_streamer
{
public:
    micro_rtsp_streamer(const uint16_t width, const uint16_t height);
    rtp_over_tcp_hdr_t *create_jpg_packet(const uint8_t *jpg_scan, const uint8_t *jpg_scan_end, uint8_t **jpg_offset, const uint32_t timestamp, const uint8_t* quantization_table_luminance, const uint8_t* quantization_table_chrominance);

private:
    uint16_t width_, height_;
    uint32_t ssrc_;
    uint16_t sequence_number_;

    // RTP data header - http://www.ietf.org/rfc/rfc3550.txt
    typedef struct __attribute__((packed))
    {
        uint16_t version : 2;   // protocol version
        uint16_t padding : 1;   // padding flag
        uint16_t extension : 1; // header extension flag
        uint16_t cc : 4;        // CSRC count
        uint16_t marker : 1;    // marker bit
        uint16_t pt : 7;        // payload type
        uint16_t seq : 16;      // sequence number
        uint32_t ts;            // timestamp
        uint32_t ssrc;          // synchronization source
    } rtp_hdr_t;

    // https://datatracker.ietf.org/doc/html/rfc2435
    typedef struct __attribute__((packed))
    {
        uint32_t tspec : 8; // type-specific field
        uint32_t off : 24;  // fragment byte offset
        uint8_t type;       // id of jpeg decoder params
        uint8_t q;          // Q values 0-127 indicate the quantization tables. JPEG types 0 and 1 (and their corresponding types 64 and 65)
        uint8_t width;      // frame width in 8 pixel blocks
        uint8_t height;     // frame height in 8 pixel blocks
    } jpeg_hdr_t;

    typedef struct __attribute__((packed))
    {
        uint16_t dri;
        uint16_t f : 1;
        uint16_t l : 1;
        uint16_t count : 14;
    } jpeg_hdr_rst_t;

    typedef struct __attribute__((packed))
    {
        uint8_t mbz;
        uint8_t precision;
        uint16_t length;
    } jpeg_hdr_qtable_t;

    // The types below will be returned, the jpeg_packet_with_quantization_t for the first packet, then the jpeg_packet_t

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
};