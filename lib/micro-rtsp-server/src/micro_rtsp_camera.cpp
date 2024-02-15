#include <esp32-hal-log.h>

#include "micro_rtsp_camera.h"

micro_rtsp_camera::micro_rtsp_camera()
{
    init_result == ESP_FAIL;
}

micro_rtsp_camera::~micro_rtsp_camera()
{
    deinitialize();
}

esp_err_t micro_rtsp_camera::initialize(camera_config_t *camera_config)
{
    init_result = esp_camera_init(camera_config);
    if (init_result == ESP_OK)
        update_frame();
    else
        log_e("Camera initialization failed: 0x%02x", init_result);

    return init_result;
}

esp_err_t micro_rtsp_camera::deinitialize()
{
    return init_result == ESP_OK ? esp_camera_deinit() : ESP_OK;
}

void micro_rtsp_camera::update_frame()
{
    if (fb)
        esp_camera_fb_return(fb);

    fb = esp_camera_fb_get();
}