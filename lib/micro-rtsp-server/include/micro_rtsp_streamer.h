#pragma once

#include <jpg.h>


class micro_rtsp_streamer
{
public:
    micro_rtsp_streamer();
    void create_jpg_packet(const jpg jpg, uint32_t timestamp);
    private :

    uint32_t ssrc_;
    uint16_t sequenceNumber_;
};