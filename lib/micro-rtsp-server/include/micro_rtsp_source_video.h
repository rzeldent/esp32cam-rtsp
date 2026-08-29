#pragma once

#include <stddef.h>
#include <stdint.h>

// Interface for a video source
class micro_rtsp_source_video
{
public:
    virtual ~micro_rtsp_source_video() = default;

    // Capture the next frame. Returns true when a new frame is available (data()/size() are valid).
    virtual void update() = 0;

    virtual uint8_t *data() const = 0;
    virtual size_t width() const = 0;
    virtual size_t height() const = 0;
    virtual size_t size() const = 0;
};