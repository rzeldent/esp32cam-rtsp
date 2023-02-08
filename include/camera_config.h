#pragma once

#include <string.h>
#include <esp_camera.h>

typedef char camera_config_name_t[11];
typedef struct
{
    const camera_config_name_t name;
    const camera_config_t config;
} camera_config_entry_t;

constexpr camera_config_t esp32cam_settings = {
    .pin_pwdn = -1,
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
    .fb_count = 2};

constexpr camera_config_t esp32cam_aithinker_settings = {
    .pin_pwdn = 32,
    .pin_reset = -1,
    .pin_xclk = 0,
    .pin_sscb_sda = 26,
    .pin_sscb_scl = 27,
    .pin_d7 = 35,
    .pin_d6 = 34,
    .pin_d5 = 39,
    .pin_d4 = 36,
    .pin_d3 = 21,
    .pin_d2 = 19,
    .pin_d1 = 18,
    .pin_d0 = 5,
    .pin_vsync = 25,
    .pin_href = 23,
    .pin_pclk = 22,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,
    .fb_count = 2};

constexpr camera_config_t esp32cam_ttgo_t_settings = {
    .pin_pwdn = 26,
    .pin_reset = -1,
    .pin_xclk = 32,
    .pin_sscb_sda = 13,
    .pin_sscb_scl = 12,
    .pin_d7 = 39,
    .pin_d6 = 36,
    .pin_d5 = 23,
    .pin_d4 = 18,
    .pin_d3 = 15,
    .pin_d2 = 4,
    .pin_d1 = 14,
    .pin_d0 = 5,
    .pin_vsync = 27,
    .pin_href = 25,
    .pin_pclk = 19,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,
    .fb_count = 2};

constexpr camera_config_t esp32cam_m5stack_settings = {
    .pin_pwdn = -1,
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
    .pin_d0 = 32,
    .pin_vsync = 22,
    .pin_href = 26,
    .pin_pclk = 21,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,
    .fb_count = 2};

constexpr camera_config_t esp32cam_wrover_kit_settings = {
    .pin_pwdn = -1,
    .pin_reset = -1,
    .pin_xclk = 21,
    .pin_sscb_sda = 26,
    .pin_sscb_scl = 27,
    .pin_d7 = 35,
    .pin_d6 = 34,
    .pin_d5 = 39,
    .pin_d4 = 36,
    .pin_d3 = 19,
    .pin_d2 = 18,
    .pin_d1 = 5,
    .pin_d0 = 4,
    .pin_vsync = 25,
    .pin_href = 23,
    .pin_pclk = 22,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SVGA,
    .jpeg_quality = 12,
    .fb_count = 2};

constexpr const camera_config_entry_t camera_configs[] = {
    {"ESP32CAM", esp32cam_settings},
    {"AI THINKER", esp32cam_aithinker_settings},
    {"TTGO T-CAM", esp32cam_ttgo_t_settings},
    {"M5 STACK", esp32cam_m5stack_settings},
    {"WROVER KIT", esp32cam_wrover_kit_settings}};

const camera_config_t lookup_camera_config(const char *name)
{
    // Lookup table for the frame name to framesize_t
    for (const auto &entry : camera_configs)
        if (strncmp(entry.name, name, sizeof(camera_config_name_t)) == 0)
            return entry.config;

    return camera_config_t{};
}

typedef char camera_effect_name_t[11];
typedef struct
{
    const camera_effect_name_t name;
    const int value;
} camera_effect_entry_t;

constexpr const camera_effect_entry_t camera_effects[] = {
    {"Normal", 0},
    {"Negative", 1},
    {"Grayscale", 2},
    {"Red tint", 3},
    {"Green tint", 4},
    {"Blue tint", 5},
    {"Sepia", 6}};

const int lookup_camera_effect(const char *name)
{
    // Lookup table for the frame name to framesize_t
    for (const auto &entry : camera_effects)
        if (strncmp(entry.name, name, sizeof(camera_effect_entry_t)) == 0)
            return entry.value;

    return 0;
}

typedef char camera_white_balance_mode_name_t[7];
typedef struct
{
    const camera_white_balance_mode_name_t name;
    const int value;
} camera_white_balance_mode_entry_t;

constexpr const camera_white_balance_mode_entry_t camera_white_balance_modes[] = {
    {"Auto", 0},
    {"Sunny", 1},
    {"Cloudy", 2},
    {"Office", 3},
    {"Home", 4}};

const int lookup_camera_white_balance_mode(const char *name)
{
    // Lookup table for the frame name to framesize_t
    for (const auto &entry : camera_white_balance_modes)
        if (strncmp(entry.name, name, sizeof(camera_white_balance_mode_entry_t)) == 0)
            return entry.value;

    return 0;
}

typedef char camera_gain_ceiling_name_t[5];
typedef struct
{
    const camera_gain_ceiling_name_t name;
    const gainceiling_t value;
} camera_gain_ceiling_entry_t;

constexpr const camera_gain_ceiling_entry_t camera_gain_ceilings[] = {
    {"2X", GAINCEILING_2X},
    {"4X", GAINCEILING_4X},
    {"8X", GAINCEILING_8X},
    {"16X", GAINCEILING_16X},
    {"32X", GAINCEILING_32X},
    {"64X", GAINCEILING_64X},
    {"128X", GAINCEILING_128X}};

const gainceiling_t lookup_camera_gain_ceiling_mode(const char *name)
{
    // Lookup table for the frame name to framesize_t
    for (const auto &entry : camera_gain_ceilings)
        if (strncmp(entry.name, name, sizeof(camera_gain_ceiling_entry_t)) == 0)
            return entry.value;

    return GAINCEILING_2X;
}
