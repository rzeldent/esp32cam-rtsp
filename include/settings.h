#pragma once

#define APP_TITLE "ESP32CAM-RTSP"
#define APP_VERSION "1.0"

#define WIFI_SSID "ESP32CAM-RTSP"
#define WIFI_PASSWORD nullptr
#define CONFIG_VERSION "1.6"

#define OTA_PASSWORD "ESP32CAM-RTSP"

// Time servers
#define NTP_SERVER_1 "nl.pool.ntp.org"
#define NTP_SERVER_2 "europe.pool.ntp.org"
#define NTP_SERVER_3 "time.nist.gov"
#define NTP_SERVERS NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3

#define RTSP_PORT 554

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