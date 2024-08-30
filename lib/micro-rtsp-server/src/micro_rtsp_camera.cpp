#include <esp32-hal-log.h>

#include "micro_rtsp_camera.h"

micro_rtsp_camera::micro_rtsp_camera()
{
    init_result_ == ESP_FAIL;
}

micro_rtsp_camera::~micro_rtsp_camera()
{
    deinitialize();
}

esp_err_t micro_rtsp_camera::initialize(camera_config_t *camera_config)
{
    log_v("camera_config={.pin_pwdn:%u,.pin_reset:%u,.pin_xclk:%u,.pin_sccb_sda:%u,.pin_sccb_scl:%u,.pin_d7:%u,.pin_d6:%u,.pin_d5:%u,.pin_d4:%u,.pin_d3:%u,.pin_d2:%u,.pin_d1:%u,.pin_d0:%u,.pin_vsync:%u,.pin_href:%u,.pin_pclk:%u,.xclk_freq_hz:%d,.ledc_timer:%u,ledc_channel:%u,.pixel_format:%d,.frame_size:%d,.jpeg_quality:%d,.fb_count:%d,.fb_location%d,.grab_mode:%d,sccb_i2c_port:%d}", camera_config->pin_pwdn, camera_config->pin_reset, camera_config->pin_xclk, camera_config->pin_sccb_sda, camera_config->pin_sccb_scl, camera_config->pin_d7, camera_config->pin_d6, camera_config->pin_d5, camera_config->pin_d4, camera_config->pin_d3, camera_config->pin_d2, camera_config->pin_d1, camera_config->pin_d0, camera_config->pin_vsync, camera_config->pin_href, camera_config->pin_pclk, camera_config->xclk_freq_hz, camera_config->ledc_timer, camera_config->ledc_channel, camera_config->pixel_format, camera_config->frame_size, camera_config->jpeg_quality, camera_config->fb_count, camera_config->fb_location, camera_config->grab_mode, camera_config->sccb_i2c_port);

    init_result_ = esp_camera_init(camera_config);
    if (init_result_ == ESP_OK)
        update_frame();
    else
        log_e("Camera initialization failed: 0x%02x", init_result_);

    return init_result_;
}

esp_err_t micro_rtsp_camera::deinitialize()
{
    return init_result_ == ESP_OK ? esp_camera_deinit() : ESP_OK;
}

void micro_rtsp_camera::update_frame()
{
    if (fb_)
        esp_camera_fb_return(fb_);

    fb_ = esp_camera_fb_get();
}