#pragma once

#include <micro_rtsp_source.h>

#include <esp_camera.h>

class micro_rtsp_source_camera : public micro_rtsp_source
{
public:
    micro_rtsp_source_camera();
    virtual ~micro_rtsp_source_camera();

    esp_err_t initialize(camera_config_t *camera_config);
    esp_err_t deinitialize();
    // sensor_t* esp_camera_sensor_get();

    void update_frame();

    uint8_t *data() const { return fb->buf; }
    size_t width() const { return fb->width; }
    size_t height() const { return fb->height; }
    size_t size() const { return fb->len; }

private:
    esp_err_t init_result;
    camera_fb_t *fb;
};