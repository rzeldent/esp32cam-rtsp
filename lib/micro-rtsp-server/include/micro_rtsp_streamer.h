#pragma once

#include <jpg.h>


class micro_rtsp_streamer
{
public:
    micro_rtsp_streamer();
    size_t create_jpg_packet(const uint8_t *jpg, const uint8_t *jpg_end, uint32_t timestamp);
    private :

    uint32_t ssrc_;
    uint16_t sequenceNumber_;
};