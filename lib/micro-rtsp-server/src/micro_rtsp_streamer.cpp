#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <esp32-hal-log.h>
#include <esp_random.h>

#include "micro_rtsp_streamer.h"

micro_rtsp_streamer::micro_rtsp_streamer(const micro_rtsp_source &source)
    : source_(source), srtp_(nullptr)
{
    video_ssrc_ = esp_random();
    video_sequence_number_ = 0;

    audio_ssrc_ = esp_random();
    audio_timestamp_ = 0;
    audio_sequence_number_ = 0;
}

uint8_t *micro_rtsp_streamer::create_jpg_packet(
    const uint8_t *jpg_scan, const uint8_t *jpg_scan_end,
    uint8_t **jpg_offset, const uint32_t timestamp,
    const uint8_t *quant_lum, const uint8_t *quant_chr,
    size_t &packet_size)
{
    log_v("jpg_scan:%p, jpg_scan_end:%p, jpg_offset:%p, timestamp:%u",
          jpg_scan, jpg_scan_end, (const void *)*jpg_offset, timestamp);

    const auto is_first_fragment = (jpg_scan == *jpg_offset);
    const auto include_quantization_tables = is_first_fragment && quant_lum != nullptr && quant_chr != nullptr;

    // Number of JPEG scan bytes in this packet, kept below the Ethernet MTU
    // so a RTP/UDP datagram is never fragmented.
    const auto jpg_bytes_left = (size_t)(jpg_scan_end - *jpg_offset);
    const auto jpg_bytes = std::min(max_jpeg_payload_size, jpg_bytes_left);
    const auto is_last_fragment = (jpg_bytes_left == jpg_bytes);

    const size_t header_size = rtp_over_tcp_hdr_size + rtp_hdr_size + jpeg_hdr_size
        + (include_quantization_tables ? (jpeg_qtable_hdr_size + 2 * jpeg_qtable_size) : 0);
    // Length of the RTP packet (without the 4 byte '$' header), before SRTP
    size_t rtp_len = header_size - rtp_over_tcp_hdr_size + jpg_bytes;
    // Reserve room for the SRTP authentication tag when enabled
    const size_t tag_size = srtp_ ? srtp_->tag_size() : 0;
    const size_t buffer_size = rtp_over_tcp_hdr_size + rtp_len + tag_size;
    if (buffer_size > sizeof(packet_buffer_))
    {
        log_e("JPEG RTP packet (%u bytes) exceeds the fixed packet buffer (%u bytes)", (unsigned)buffer_size, (unsigned)sizeof(packet_buffer_));
        return nullptr;
    }

    // Build the packet in the streamer's fixed buffer: no per-packet heap
    // allocation, avoiding malloc/free churn (and heap fragmentation) while
    // streaming a frame.
    auto packet = packet_buffer_;
    memset(packet, 0, buffer_size);
    auto p = packet;

    // ---- RTP over TCP framing header ($, channel; length is set at the end,
    //      after optional SRTP protection has grown the RTP packet) ----
    *p++ = '$';
    *p++ = 0; // video RTP channel
    p += 2;

    // ---- RTP header (RFC 3550), all fields network byte order ----
    *p++ = 0x80;                                                                     // V=2, P=0, X=0, CC=0
    *p++ = RTP_PAYLOAD_JPG | (is_last_fragment ? 0x80 : 0x00);                       // marker + payload type
    *p++ = (uint8_t)(video_sequence_number_ >> 8);                                   // sequence number
    *p++ = (uint8_t)(video_sequence_number_ & 0xff);
    *p++ = (uint8_t)(timestamp >> 24);                                               // timestamp (90 kHz)
    *p++ = (uint8_t)((timestamp >> 16) & 0xff);
    *p++ = (uint8_t)((timestamp >> 8) & 0xff);
    *p++ = (uint8_t)(timestamp & 0xff);
    *p++ = (uint8_t)(video_ssrc_ >> 24);                                             // synchronization source
    *p++ = (uint8_t)((video_ssrc_ >> 16) & 0xff);
    *p++ = (uint8_t)((video_ssrc_ >> 8) & 0xff);
    *p++ = (uint8_t)(video_ssrc_ & 0xff);

    // ---- JPEG payload header (RFC 2435) ----
    const uint32_t fragment_offset = (uint32_t)(*jpg_offset - jpg_scan);
    *p++ = 0x00;                                                     // type-specific field
    *p++ = (uint8_t)((fragment_offset >> 16) & 0xff);                // fragment offset (24 bit)
    *p++ = (uint8_t)((fragment_offset >> 8) & 0xff);
    *p++ = (uint8_t)(fragment_offset & 0xff);
    *p++ = 0x00;                                                     // type: standard baseline JPEG
    *p++ = include_quantization_tables ? 0x80 : 0x5e;                // q: 0x80 = tables follow, otherwise 94
    *p++ = (uint8_t)(source_.width() >> 3);                          // frame width in 8 pixel blocks
    *p++ = (uint8_t)(source_.height() >> 3);                         // frame height in 8 pixel blocks

    // ---- Quantization tables (only in the first fragment of a frame) ----
    if (include_quantization_tables)
    {
        *p++ = 0x00; // MBZ
        *p++ = 0x00; // 8 bit precision
        const uint16_t table_length = 2 * jpeg_qtable_size;
        *p++ = (uint8_t)(table_length >> 8);
        *p++ = (uint8_t)(table_length & 0xff);
        memcpy(p, quant_lum, jpeg_qtable_size);
        p += jpeg_qtable_size;
        memcpy(p, quant_chr, jpeg_qtable_size);
        p += jpeg_qtable_size;
    }

    // ---- JPEG scan data ----
    memcpy(p, *jpg_offset, jpg_bytes);

    // ---- Optional SRTP protection (encrypts the payload, appends a tag) ----
    if (srtp_)
        srtp_->protect_rtp(packet + rtp_over_tcp_hdr_size, rtp_len);

    // Total size of the buffer handed to the caller (4 byte '$' header +
    // the possibly protected RTP packet)
    packet_size = rtp_over_tcp_hdr_size + rtp_len;

    // RTP over TCP framing header: length of the (possibly protected) RTP packet
    packet[2] = (uint8_t)(rtp_len >> 8);
    packet[3] = (uint8_t)(rtp_len & 0xff);

    // Advance the scan offset and RTP sequence number
    *jpg_offset += jpg_bytes;
    video_sequence_number_++;

    log_v("packet_size:%u, fragment_offset:%u, is_first:%u, is_last:%u, srtp:%u",
          packet_size, fragment_offset, is_first_fragment, is_last_fragment, srtp_ != nullptr);
    return packet;
}

uint8_t *micro_rtsp_streamer::create_audio_packet(
    const uint8_t *data, size_t len, bool marker, size_t &packet_size)
{
    const size_t header_size = rtp_over_tcp_hdr_size + rtp_hdr_size;
    // Length of the RTP packet (without the 4 byte '$' header), before SRTP
    size_t rtp_len = header_size - rtp_over_tcp_hdr_size + len;
    // Reserve room for the SRTP authentication tag when enabled
    const size_t tag_size = srtp_ ? srtp_->tag_size() : 0;
    const size_t buffer_size = rtp_over_tcp_hdr_size + rtp_len + tag_size;
    if (buffer_size > sizeof(packet_buffer_))
    {
        log_e("Audio RTP packet (%u bytes) exceeds the fixed packet buffer (%u bytes)", (unsigned)buffer_size, (unsigned)sizeof(packet_buffer_));
        return nullptr;
    }

    // Build the packet in the streamer's fixed buffer (see create_jpg_packet).
    auto packet = packet_buffer_;
    memset(packet, 0, buffer_size);
    auto p = packet;

    // ---- RTP over TCP framing header ($, channel; length set at the end) ----
    *p++ = '$';
    *p++ = 2; // audio RTP channel
    p += 2;

    // ---- RTP header (RFC 3550), all fields network byte order ----
    *p++ = 0x80;                                                       // V=2, P=0, X=0, CC=0
    *p++ = RTP_PAYLOAD_PCMA | (marker ? 0x80 : 0x00);                  // marker + payload type
    *p++ = (uint8_t)(audio_sequence_number_ >> 8);                     // sequence number
    *p++ = (uint8_t)(audio_sequence_number_ & 0xff);
    *p++ = (uint8_t)(audio_timestamp_ >> 24);                          // timestamp (8 kHz)
    *p++ = (uint8_t)((audio_timestamp_ >> 16) & 0xff);
    *p++ = (uint8_t)((audio_timestamp_ >> 8) & 0xff);
    *p++ = (uint8_t)(audio_timestamp_ & 0xff);
    *p++ = (uint8_t)(audio_ssrc_ >> 24);                               // synchronization source
    *p++ = (uint8_t)((audio_ssrc_ >> 16) & 0xff);
    *p++ = (uint8_t)((audio_ssrc_ >> 8) & 0xff);
    *p++ = (uint8_t)(audio_ssrc_ & 0xff);

    // ---- a-law samples ----
    memcpy(p, data, len);

    // ---- Optional SRTP protection (encrypts the payload, appends a tag) ----
    if (srtp_)
        srtp_->protect_rtp(packet + rtp_over_tcp_hdr_size, rtp_len);

    // Total size of the buffer handed to the caller (4 byte '$' header +
    // the possibly protected RTP packet)
    packet_size = rtp_over_tcp_hdr_size + rtp_len;

    // RTP over TCP framing header: length of the (possibly protected) RTP packet
    packet[2] = (uint8_t)(rtp_len >> 8);
    packet[3] = (uint8_t)(rtp_len & 0xff);

    audio_sequence_number_++;
    audio_timestamp_ += (uint32_t)len; // one byte == one 8 kHz sample

    return packet;
}