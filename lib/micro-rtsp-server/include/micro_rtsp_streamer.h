#pragma once

#include <stddef.h>
#include <stdint.h>

#include <micro_rtsp_source_video.h>
#include <micro_rtsp_structs.h>
#include <micro_rtsp_srtp.h>

// Streams JPEG frames (and optional G.711 a-law audio) from a source as a
// sequence of RTP packets. One streamer instance keeps the RTP state
// (synchronization source, sequence numbers and timestamps) for a single
// client, so every connected client needs its own streamer.
class micro_rtsp_streamer
{
public:
    explicit micro_rtsp_streamer(const micro_rtsp_source_video &source);

    // Enable SRTP (RFC 3711) protection of the generated RTP packets. When set, every packet is encrypted and carries an authentication tag.
    void set_srtp(micro_rtsp_srtp *srtp);
    micro_rtsp_srtp *srtp() const ;

    // Create a single RTP/JPEG packet for the current JPEG fragment.
    //   jpg_scan     - pointer to the first byte of the JPEG scan data
    //   jpg_scan_end - pointer just past the last byte of the scan data
    //   jpg_offset   - in/out: current position, advanced by the number of
    //                  scan bytes packed into this packet
    //   timestamp    - RTP timestamp (90 kHz clock) for the whole frame
    //   quant_lum / quant_chr - quantization tables, only used in the first
    //                  packet of a frame (may be nullptr)
    //   packet_size  - out: total size of the returned buffer (including the
    //                  4 byte RTP-over-TCP header and any SRTP tag)
    // Returns a pointer into the streamer's fixed internal buffer, which
    // starts with the '$' RTP-over-TCP header followed by the RTP packet.
    // For UDP transport skip the first 4 bytes. The buffer is only valid
    // until the next create_jpg_packet()/create_audio_packet() call on the
    // same streamer and must NOT be freed by the caller.
    uint8_t *create_jpg_packet(const uint8_t *jpg_scan, const uint8_t *jpg_scan_end, uint8_t **jpg_offset, uint32_t timestamp, const uint8_t *quant_lum, const uint8_t *quant_chr, size_t &packet_size);

    // Create a single RTP packet carrying G.711 a-law samples (payload 8).
    //   data        - a-law encoded samples (one byte per 8 kHz sample)
    //   len         - number of samples
    //   marker      - RTP marker bit (set on the first packet of a talkspurt)
    //   packet_size - out: total size of the returned buffer (including the
    //                 4 byte RTP-over-TCP header and any SRTP tag)
    // Same ownership semantics as create_jpg_packet(): the returned pointer
    // points into the streamer's fixed buffer and must NOT be freed.
    uint8_t *create_audio_packet(const uint8_t *data, size_t len, bool marker, size_t &packet_size);

    static size_t max_payload_size() { return max_jpeg_payload_size; }

private:
    const micro_rtsp_source_video &source_;
    micro_rtsp_srtp *srtp_;

    uint32_t video_ssrc_;
    uint16_t video_sequence_number_;

    uint32_t audio_ssrc_;
    uint32_t audio_timestamp_;
    uint16_t audio_sequence_number_;

    // Fixed-size buffer used to build each RTP packet. Sized for the largest possible packet: a JPEG fragment including quantization tables and the SRTP authentication tag.
    // Reusing a single buffer avoids a malloc/free per RTP packet, which otherwise fragments the heap while streaming.
    static constexpr size_t max_packet_buffer_size = rtp_over_tcp_hdr_size + rtp_hdr_size + jpeg_hdr_size + jpeg_qtable_hdr_size + 2 * jpeg_qtable_size + max_jpeg_payload_size + micro_rtsp_srtp::auth_tag_size;

    uint8_t packet_buffer_[max_packet_buffer_size];
};