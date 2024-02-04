#pragma once

#define APP_TITLE "ESP32CAM-RTSP"
#define APP_VERSION "1.0"

#define WIFI_SSID "ESP32CAM-RTSP"
#define WIFI_PASSWORD nullptr
#define CONFIG_VERSION "1.6"

#define OTA_PASSWORD "ESP32CAM-RTSP"

#define RTSP_PORT 554

#if 0
#if defined(BOARD_ESP32CAM)
constexpr const char *board_name = "ESP32CAM";
constexpr camera_config_t default_camera_config = esp32cam_camera_settings;
#elif defined(BOARD_AITHINKER_ESP32CAM)
constexpr const char *board_name = "AI-Thinker ESP32CAM";
constexpr camera_config_t default_camera_config = aithinker_camera_settings;
#elif defined(BOARD_ESP_EYE)
constexpr const char *board_name = "ESP-EYE";
constexpr camera_config_t default_camera_config = esp_eye_camera_settings;
#elif defined(BOARD_TTGO_T_CAMERA)
constexpr const char *board_name = "TTGO-T-CAMERA";
constexpr camera_config_t default_camera_config = ttgo_t_camera_settings;
#elif defined(BOARD_M5STACK_ESP32CAM)
constexpr const char *board_name = "M5STACK-CAMERA";
constexpr camera_config_t default_camera_config = m5stack_camera_settings;
#elif defined(BOARD_ESP32_WROVER_CAM)
constexpr const char *board_name = "WROVER-KIT";
constexpr camera_config_t default_camera_config = wrover_kit_camera_settings;
#elif defined(BOARD_SEEED_XIAO_ESP32S3_SENSE)
constexpr const char *board_name = "Seed Xiao ESP32S3 Sense";
constexpr camera_config_t default_camera_config = xiao_esp32s3_camera_settings;
#elif defined(BOARD_M5STACK_UNITCAMS3)
constexpr const char *board_name = "M5Stack UnitCamS3";
constexpr camera_config_t default_camera_config = m5stack_unitcams3_camera_settings;
#else
#error No board defined
#endif
#endif

#define DEFAULT_FRAME_DURATION 200
#define DEFAULT_FRAME_SIZE "VGA (640x480)"
#define DEFAULT_JPEG_QUALITY (psramFound() ? 12 : 14)

#define DEFAULT_BRIGHTNESS 0
#define DEFAULT_CONTRAST 0
#define DEFAULT_SATURATION 0
#define DEFAULT_EFFECT "Normal"
#define DEFAULT_WHITE_BALANCE true
#define DEFAULT_WHITE_BALANCE_GAIN true
#define DEFAULT_WHITE_BALANCE_MODE "Auto"
#define DEFAULT_EXPOSURE_CONTROL true
#define DEFAULT_AEC2 true
#define DEFAULT_AE_LEVEL 0
#define DEFAULT_AEC_VALUE 300
#define DEFAULT_GAIN_CONTROL true
#define DEFAULT_AGC_GAIN 0
#define DEFAULT_GAIN_CEILING "2X"
#define DEFAULT_BPC false
#define DEFAULT_WPC true
#define DEFAULT_RAW_GAMMA true
#define DEFAULT_LENC true
#define DEFAULT_HORIZONTAL_MIRROR false
#define DEFAULT_VERTICAL_MIRROR false
#define DEFAULT_DCW true
#define DEFAULT_COLORBAR false

#define DEFAULT_LED_INTENSITY 0
