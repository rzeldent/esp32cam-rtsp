#include <Arduino.h>

#include <soc/rtc_cntl_reg.h>

#include <IotWebConf.h>
#include <IotWebConfTParameter.h>

#include <OV2640.h>

#include <rtsp_server.h>

#include <frame_size.h>

#include <esp32cam.h>

char frame_rate_val[6];
char frame_size_val[sizeof(frame_size_entry_t)];

auto config_group_stream_settings = iotwebconf::ParameterGroup("settings", "Streaming settings");
auto config_frame_rate = iotwebconf::NumberParameter("Frame rate (ms)", "fr", frame_rate_val, sizeof(frame_rate_val), DEFAULT_FRAMERATE);
auto config_frame_size = iotwebconf::SelectParameter("Frame size", "fs", frame_size_val, sizeof(frame_size_val), (const char *)frame_sizes, (const char *)frame_sizes, sizeof(frame_sizes) / sizeof(frame_sizes[0]), sizeof(frame_sizes[0]), DEFAULT_FRAMESIZE);

// Camera
OV2640 cam;

// DNS Server
DNSServer dnsServer;

// RTSP Server
std::unique_ptr<rtsp_server> camera_server;

// Web server
WebServer web_server(80);
IotWebConf iotWebConf(WIFI_SSID, &dnsServer, &web_server, WIFI_PASSWORD, CONFIG_VERSION);

void handleRoot()
{
  log_v("Handle root");
  // Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
    return;

  String html;
  html += "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  html += "<title>" APP_TITLE " v" APP_VERSION "</title></head>";
  html += "<body>";
  html += "<h2>Status page for " + String(iotWebConf.getThingName()) + "</h2><hr />";
  html += "<h3>ESP32</h3>";
  html += "<ul>";
  html += "<li>CPU model: " + String(ESP.getChipModel()) + "</li>";
  html += "<li>CPU speed: " + String(ESP.getCpuFreqMHz()) + "Mhz</li>";
  html += "<li>Mac address: " + WiFi.macAddress() + "</li>";
  html += "</ul>";
  html += "<h3>Settings</h3>";
  html += "<ul>";
  html += "<li>Frame size: " + String(frame_size_val) + "</li>";
  html += "<li>Frame rate: " + String(frame_rate_val) + "</li>";
  html += "</ul>";
  html += "<br/>Go to <a href=\"config\">configure page</a> to change settings.";
  html += "</body></html>";
  web_server.send(200, "text/html", html);
}

void on_config_saved()
{
  log_v("on_config_saved");
  ESP.restart();
}

void initialize_camera()
{
  log_v("Initialize camera");
  log_i("Frame size: %s", frame_size_val);
  auto frame_size = lookup_frame_size(frame_size_val);
  // ESP32CAM
  log_d("Looking for ESP32CAM");
  esp32cam_config.frame_size = frame_size;
  if (cam.init(esp32cam_aithinker_config) == ESP_OK)
  {
    log_i("Found ESP32CAM");
    return;
  }

  // AI Thinker
  log_d("Looking for AI Thinker");
  esp32cam_aithinker_config.frame_size = frame_size;
  if (cam.init(esp32cam_aithinker_config) == ESP_OK)
  {
    log_i("Found AI Thinker");
    return;
  }

  // TTGO T-Cam
  log_d("Looking for TTGO T-Cam");
  esp32cam_ttgo_t_config.frame_size = frame_size;
  if (cam.init(esp32cam_ttgo_t_config) == ESP_OK)
  {
    log_i("Found TTGO T-Cam");
    return;
  }

  log_e("No camera found");
}

void start_rtsp_server()
{
  log_v("start_rtsp_server");
  initialize_camera();
  auto frame_rate = atol(frame_rate_val);
  camera_server = std::unique_ptr<rtsp_server>(new rtsp_server(cam, frame_rate, RTSP_PORT));
}

void on_connected()
{
  log_v("on_connected");
  start_rtsp_server();
}

void setup()
{
  // Disable brownout
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, false);

#ifdef CORE_DEBUG_LEVEL
  Serial.begin(115200);
  Serial.setDebugOutput(true);

#endif

  log_i("CPU Freq = %d Mhz", getCpuFrequencyMhz());
  log_i("Free heap: %d bytes", ESP.getFreeHeap());
  log_i("Starting " APP_TITLE "...");

  config_group_stream_settings.addItem(&config_frame_rate);
  config_group_stream_settings.addItem(&config_frame_size);
  iotWebConf.addParameterGroup(&config_group_stream_settings);
  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.setConfigSavedCallback(on_config_saved);
  iotWebConf.setWifiConnectionCallback(on_connected);

  iotWebConf.init();

  // Set up required URL handlers on the web server
  web_server.on("/", HTTP_GET, handleRoot);
  web_server.on("/config", []
                { iotWebConf.handleConfig(); });
  web_server.onNotFound([]()
                        { iotWebConf.handleNotFound(); });
}

void loop()
{
  iotWebConf.doLoop();

  if (camera_server)
    camera_server->doLoop();
}