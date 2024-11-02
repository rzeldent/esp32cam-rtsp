#pragma once

#include <micro_rtsp_source.h>
#include <esp_camera.h>

class micro_rtsp_camera : public micro_rtsp_source
{
public:
    micro_rtsp_camera();
    virtual ~micro_rtsp_camera();

    esp_err_t initialize(camera_config_t *camera_config);
    esp_err_t deinitialize();

    virtual void update_frame();

    virtual uint8_t *data() const { return fb_->buf; }
    virtual size_t width() const { return fb_->width; }
    virtual size_t height() const { return fb_->height; }
    virtual size_t size() const { return fb_->len; }

private:
    esp_err_t init_result_;
    camera_fb_t *fb_;
};