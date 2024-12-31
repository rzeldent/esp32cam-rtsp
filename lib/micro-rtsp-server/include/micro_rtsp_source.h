#pragma once

#include <stddef.h>
#include <stdint.h>

// Interface for a video source
class micro_rtsp_source
{
public:
    virtual void update_frame() = 0;

    virtual uint8_t *data() const = 0;
    virtual size_t width() const = 0;
    virtual size_t height() const = 0;
    virtual size_t size() const = 0;
};