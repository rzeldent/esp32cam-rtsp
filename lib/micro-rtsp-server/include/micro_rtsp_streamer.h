#pragma once

#include <jpg.h>

class micro_rtsp_streamer
{
public:
    micro_rtsp_streamer(const uint16_t width, const uint16_t height);
    size_t create_jpg_packet(const uint8_t *jpg, uint8_t **jpg_current, const uint8_t *jpg_end, const uint32_t timestamp);

private:
    uint16_t width_, height_;
    uint32_t ssrc_;
    uint16_t sequenceNumber_;
};