#pragma once

#include <string.h>
#include <sensor.h>

typedef char camera_config_name_t[18];

typedef struct camera_config_entry
{
    const camera_config_name_t name;
    const camera_config_t config;
} camera_config_entry_t;

constexpr const camera_config_entry_t camera_configs[] = {

    {"ESP32CAM",
     {.pin_pwdn = -1,
      .pin_reset = 15,
      .pin_xclk = 27,
      .pin_sscb_sda = 25,
      .pin_sscb_scl = 23,
      .pin_d7 = 19,
      .pin_d6 = 36,
      .pin_d5 = 18,
      .pin_d4 = 39,
      .pin_d3 = 5,
      .pin_d2 = 34,
      .pin_d1 = 35,
      .pin_d0 = 17,
      .pin_vsync = 22,
      .pin_href = 26,
      .pin_pclk = 21,
      .xclk_freq_hz = 20000000,
      .ledc_timer = LEDC_TIMER_0,
      .ledc_channel = LEDC_CHANNEL_0,
      .pixel_format = PIXFORMAT_JPEG,
      .frame_size = FRAMESIZE_SVGA,
      .jpeg_quality = 12,
      .fb_count = 2}},
    {"AI THINKER", 
    {.pin_pwdn = 32,
     .pin_reset = -1, .pin_xclk = 0, .pin_sscb_sda = 26, .pin_sscb_scl = 27, .pin_d7 = 35, .pin_d6 = 34, .pin_d5 = 39, .pin_d4 = 36, .pin_d3 = 21, .pin_d2 = 19, .pin_d1 = 18, .pin_d0 = 5, .pin_vsync = 25, .pin_href = 23, .pin_pclk = 22, .xclk_freq_hz = 20000000, .ledc_timer = LEDC_TIMER_1, .ledc_channel = LEDC_CHANNEL_1, .pixel_format = PIXFORMAT_JPEG, .frame_size = FRAMESIZE_SVGA, .jpeg_quality = 12, .fb_count = 2}},
    {"TTGO T-CAM",
     {.pin_pwdn = 26,
      .pin_reset = -1, .pin_xclk = 32, .pin_sscb_sda = 13, .pin_sscb_scl = 12, .pin_d7 = 39, .pin_d6 = 36, .pin_d5 = 23, .pin_d4 = 18, .pin_d3 = 15, .pin_d2 = 4, .pin_d1 = 14, .pin_d0 = 5, .pin_vsync = 27, .pin_href = 25, .pin_pclk = 19, .xclk_freq_hz = 20000000, .ledc_timer = LEDC_TIMER_0, .ledc_channel = LEDC_CHANNEL_0, .pixel_format = PIXFORMAT_JPEG, .frame_size = FRAMESIZE_SVGA, .jpeg_quality = 12, .fb_count = 2}}};

const camera_config_t lookup_camera_config(const char *pin)
{
    // Lookup table for the frame name to framesize_t
    for (const auto &entry : camera_configs)
        if (strncmp(entry.name, pin, sizeof(camera_config_name_t)) == 0)
            return entry.config;

    return camera_config_t{};
}