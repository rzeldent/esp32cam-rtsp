#include <stddef.h>
#include <memory.h>
#include <esp32-hal-log.h>

#include "micro_rtsp_streamer.h"
#include "esp_random.h"

micro_rtsp_streamer::micro_rtsp_streamer(const uint16_t width, const uint16_t height)
{
    width_ = width;
    height_ = height;
    // Random number
    ssrc_ = esp_random();
    sequence_number_ = 0;
}

rtp_over_tcp_hdr_t *micro_rtsp_streamer::create_jpg_packet(const uint8_t *jpg_scan, const uint8_t *jpg_scan_end, uint8_t **jpg_offset, const uint32_t timestamp, const uint8_t* quantization_table_luminance, const uint8_t* quantization_table_chrominance)
{
    // The MTU of wireless networks is 2,312 bytes. This size includes the packet headers.
    const auto isFirstFragment = jpg_scan == *jpg_offset;
    const auto include_quantization_tables = isFirstFragment && quantization_table_luminance != nullptr && quantization_table_chrominance != nullptr;
    // Quantization tables musty be included in the first packet
    const auto headers_size = include_quantization_tables ? sizeof(jpeg_packet_with_quantization_t) : sizeof(jpeg_packet_t);
    const auto payload_size = max_wifi_mtu - headers_size;

    const auto jpg_bytes_left = jpg_scan_end - *jpg_offset;
    const bool isLastFragment = jpg_bytes_left <= payload_size;
    const auto jpg_bytes = isLastFragment ? jpg_bytes_left : payload_size;
    const uint16_t packet_size = headers_size + jpg_bytes;

    const auto packet = (jpeg_packet_t *)calloc(1, packet_size);

    // 4 bytes RTP over TCP header
    assert(4 == sizeof(rtp_over_tcp_hdr_t));
    packet->rtp_over_tcp_hdr.magic = '$'; // encapsulation
    packet->rtp_over_tcp_hdr.channel = 0; // number of multiplexed sub-channel on RTPS connection - here the RTP channel
    packet->rtp_over_tcp_hdr.length = packet_size;
    log_v("rtp_over_tcp_hdr_t={.magic=%c,.channel=%i,.length=%i}", packet->rtp_over_tcp_hdr.magic, packet->rtp_over_tcp_hdr.channel, packet->rtp_over_tcp_hdr.length);

    // 12 bytes RTP header
    assert(12 == sizeof(rtp_hdr_t));
    packet->rtp_hdr.version = 2;
    packet->rtp_hdr.marker = isLastFragment;
    packet->rtp_hdr.pt = RTP_PAYLOAD_JPG;
    packet->rtp_hdr.seq = sequence_number_;
    packet->rtp_hdr.ts = timestamp;
    packet->rtp_hdr.ssrc = ssrc_;
    log_v("rtp_hdr={.version:%i,.padding:%i,.extension:%i,.cc:%i,.marker:%i,.pt:%i,.seq:%i,.ts:%u,.ssrc:%u}", packet->rtp_hdr.version, packet->rtp_hdr.padding, packet->rtp_hdr.extension, packet->rtp_hdr.cc, packet->rtp_hdr.marker, packet->rtp_hdr.pt, packet->rtp_hdr.seq, packet->rtp_hdr.ts, packet->rtp_hdr.ssrc);

    // 8 bytes JPEG payload header
    assert(8 == sizeof(jpeg_hdr_t));
    packet->jpeg_hdr.tspec = 0;                                                // type-specific field
    packet->jpeg_hdr.off = (uint32_t)(*jpg_offset - jpg_scan);                 // fragment byte offset (24 bits in jpg)
    packet->jpeg_hdr.type = 0;                                                 // id of jpeg decoder params
    packet->jpeg_hdr.q = (uint8_t)(include_quantization_tables ? 0x80 : 0x5e); // quantization factor (or table id) 5eh=94d
    packet->jpeg_hdr.width = (uint8_t)(width_ >> 3);                           // frame width in 8 pixel blocks
    packet->jpeg_hdr.height = (uint8_t)(height_ >> 3);                         // frame height in 8 pixel blocks
    log_v("jpeg_hdr={.tspec:%i,.off:0x%6x,.type:0x2%x,.q:%i,.width:%i.height:%i}", packet->jpeg_hdr.tspec, packet->jpeg_hdr.off, packet->jpeg_hdr.type, packet->jpeg_hdr.q, packet->jpeg_hdr.width, packet->jpeg_hdr.height);

    // length so far should be 24 bytes
    assert(24 == sizeof(jpeg_packet_t));

    if (include_quantization_tables)
    {
        const auto packet_with_quantization = (jpeg_packet_with_quantization_t *)packet;
        assert(4 == sizeof(jpeg_hdr_qtable_t));
        // Only in first packet of the frame
        packet_with_quantization->jpeg_hdr_qtable.mbz = 0;
        packet_with_quantization->jpeg_hdr_qtable.precision = 0; // 8 bit precision
        packet_with_quantization->jpeg_hdr_qtable.length = jpeg_luminance_table_length + jpeg_chrominance_table_length;
        log_v("jpeg_hdr_qtable={.mbz:%i,.precision:%i,.length:%d}", packet_with_quantization->jpeg_hdr_qtable.mbz, packet_with_quantization->jpeg_hdr_qtable.precision, packet_with_quantization->jpeg_hdr_qtable.length);
        memcpy(packet_with_quantization->quantization_table_luminance, quantization_table_luminance, jpeg_luminance_table_length);
        memcpy(packet_with_quantization->quantization_table_chrominance, quantization_table_chrominance, jpeg_chrominance_table_length);
        // Copy JPG data
        memcpy(packet_with_quantization->jpeg_data, *jpg_offset, jpg_bytes);
    }
    else
    {
        // Copy JPG data
        memcpy(packet->jpeg_data, *jpg_offset, jpg_bytes);
    }

    // Update JPG offset
    *jpg_offset += jpg_bytes;
    // Update sequence number
    sequence_number_++;

    return (rtp_over_tcp_hdr_t*)packet;
}