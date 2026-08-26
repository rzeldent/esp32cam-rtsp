#pragma once

#include <micro_rtsp_source.h>
#include <esp_camera.h>

// Video source that captures JPEG frames using the esp32-camera driver.
class micro_rtsp_camera : public micro_rtsp_source
{
public:
    micro_rtsp_camera();
    virtual ~micro_rtsp_camera();

    esp_err_t initialize(camera_config_t *camera_config);
    esp_err_t deinitialize();

    bool available() const { return init_result_ == ESP_OK && fb_ != nullptr; }

    virtual void update_frame();

    virtual uint8_t *data() const { return fb_ ? fb_->buf : nullptr; }
    virtual size_t width() const { return fb_ ? fb_->width : 0; }
    virtual size_t height() const { return fb_ ? fb_->height : 0; }
    virtual size_t size() const { return fb_ ? fb_->len : 0; }

private:
    esp_err_t init_result_;
    camera_fb_t *fb_;
};