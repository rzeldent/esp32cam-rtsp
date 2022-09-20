#include <Arduino.h>
#include <ArduinoOTA.h>
#include <soc/rtc_cntl_reg.h>
#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include <OV2640.h>
#include <ESPmDNS.h>
#include <rtsp_server.h>
#include <frame_size.h>
#include <camera_config.h>
#include <format_duration.h>
#include <format_number.h>
#include <template_render.h>
#include <html_data.h>
#include <settings.h>

char camera_config_val[sizeof(camera_config_entry)];
char frame_duration_val[6];
char frame_size_val[sizeof(frame_size_entry_t)];
char frame_buffers_val[3];
char jpeg_quality_val[4];

auto config_group_stream_settings = iotwebconf::ParameterGroup("settings", "Streaming settings");
auto config_camera_config = iotwebconf::SelectParameter("Camera config", "config", camera_config_val, sizeof(camera_config_val), (const char *)camera_configs, (const char *)camera_configs, sizeof(camera_configs) / sizeof(camera_configs[0]), sizeof(camera_configs[0]), DEFAULT_CAMERA_CONFIG);
auto config_frame_rate = iotwebconf::NumberParameter("Frame duration (ms)", "fd", frame_duration_val, sizeof(frame_duration_val), DEFAULT_FRAME_DURATION, nullptr, "min=\"10\"");
auto config_frame_size = iotwebconf::SelectParameter("Frame size", "fs", frame_size_val, sizeof(frame_size_val), (const char *)frame_sizes, (const char *)frame_sizes, sizeof(frame_sizes) / sizeof(frame_sizes[0]), sizeof(frame_sizes[0]), DEFAULT_FRAME_SIZE);
auto config_frame_buffers = iotwebconf::NumberParameter("Frame buffers", "fb", frame_buffers_val, sizeof(frame_buffers_val), DEFAULT_FRAME_BUFFERS, nullptr, "min=\"1\" max=\"16\"");
auto config_jpg_quality = iotwebconf::NumberParameter("JPEG quality", "q", jpeg_quality_val, sizeof(jpeg_quality_val), DEFAULT_JPEG_QUALITY, nullptr, "min=\"1\" max=\"100\"");

// Camera
OV2640 cam;
// DNS Server
DNSServer dnsServer;
// RTSP Server
std::unique_ptr<rtsp_server> camera_server;
// Web server
WebServer web_server(80);
IotWebConf iotWebConf(WIFI_SSID, &dnsServer, &web_server, WIFI_PASSWORD, CONFIG_VERSION);

// Keep track of config changes. This will allow a reset of the device
bool config_changed = false;
// Camera initialization result
esp_err_t camera_init_result;

void stream_text_file(const char *contents, const char *mime_type)
{
  // Cache for 86400 seconds (one day)
  web_server.sendHeader("Cache-Control", "max-age=86400");
  web_server.send(200, mime_type, contents);
}

void handle_root()
{
  log_v("Handle root");
  // Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
    return;

  const char *wifi_modes[] = {"NULL", "STA", "AP", "STA+AP"};

  const template_variable_t substitutions[] = {
      // Config Changed?
      {"ConfigChanged", String(config_changed)},
      // Version / CPU
      {"AppTitle", APP_TITLE},
      {"AppVersion", APP_VERSION},
      {"ThingName", iotWebConf.getThingName()},
      {"ChipModel", ESP.getChipModel()},
      {"ChipRevision", String(ESP.getChipRevision())},
      {"CpuFreqMHz", String(ESP.getCpuFreqMHz())},
      {"CpuCores", String(ESP.getChipCores())},
      {"FlashSize", format_memory(ESP.getFlashChipSize(), 0)},
      {"HeapSize", format_memory(ESP.getHeapSize())},
      {"PsRamSize", format_memory(ESP.getPsramSize(), 0)},
      // Diagnostics
      {"Uptime", String(format_duration(millis() / 1000))},
      {"FreeHeap", format_memory(ESP.getFreeHeap())},
      {"MaxAllocHeap", format_memory(ESP.getMaxAllocHeap())},
      {"NumRTSPSessions", camera_server != nullptr ? String(camera_server->num_connected()) : "N/A"},
      // Network
      {"MacAddress", WiFi.macAddress()},
      {"AccessPoint", WiFi.SSID()},
      {"SignalStrength", String(WiFi.RSSI())},
      {"IpV4", WiFi.localIP().toString()},
      {"IpV6", WiFi.localIPv6().toString()},
      {"WifiMode", wifi_modes[WiFi.getMode()]},
      {"NetworkState.ApMode", String(iotWebConf.getState() == iotwebconf::NetworkState::ApMode)},
      {"NetworkState.OnLine", String(iotWebConf.getState() == iotwebconf::NetworkState::OnLine)},
      // Camera
      {"CameraType", camera_config_val},
      {"FrameSize", frame_size_val},
      {"FrameDuration", frame_duration_val},
      {"FrameFrequency", String(1000.0 / atol(frame_duration_val), 1)},
      {"FrameBufferLocation", psramFound() ? "PSRAM" : "DRAM)"},
      {"FrameBuffers", frame_buffers_val},
      {"JpegQuality", jpeg_quality_val},
      {"CameraInitialized", String(camera_init_result == ESP_OK)},
      {"CameraInitResult", "0x" + String(camera_init_result, 16)},
      {"CameraInitResultText", esp_err_to_name(camera_init_result)},
      // RTSP
      {"RtspPort", String(RTSP_PORT)}};

  web_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  auto html = template_render(file_data_index_html, substitutions);
  web_server.send(200, "text/html", html);
}

void handle_restart()
{
  log_v("Handle restart");
  // If configuration is not changed and camera working, do not allow a restart
  if (!config_changed && camera_init_result == ESP_OK)
  {
    // Redirect to root page
    web_server.sendHeader("Location", "/", true);
    web_server.send(302, "text/plain", "Restart not possible.");
    return;
  }

  const template_variable_t substitutions[] = {
      {"AppTitle", APP_TITLE},
      {"AppVersion", APP_VERSION},
      {"ThingName", iotWebConf.getThingName()}};

  auto html = template_render(file_data_restart_html, substitutions);
  web_server.send(200, "text/html", html);
  log_v("Restarting... Press refresh to connect again");
  sleep(100);
  ESP.restart();
}

void handle_snapshot()
{
  log_v("handle_jpg");
  if (camera_init_result != ESP_OK)
  {
    web_server.send(404, "text/plain", "Camera is not initialized");
    return;
  }

  web_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  web_server.sendContent("HTTP/1.1 200 OK\r\n"
                         "Content-Disposition: inline; filename=snapshot.jpg\r\n"
                         "Content-Type: image/jpeg\r\n\r\n");
  // Make a copy in memory to prevent interaction with RTSP
  auto fb_len = cam.getSize();
  cam.run();
  auto fb = (uint8_t *)memcpy(new uint8_t[cam.getSize()], cam.getfb(), fb_len);
  web_server.sendContent(reinterpret_cast<const char *>(fb), fb_len);
  delete[] fb;
}

void on_config_saved()
{
  log_v("on_config_saved");
  config_changed = true;
}

esp_err_t initialize_camera()
{
  log_v("initialize_camera");
  log_i("Camera config: %s", camera_config_val);
  auto camera_config = lookup_camera_config(camera_config_val);
  log_i("Frame size: %s", frame_size_val);
  auto frame_size = lookup_frame_size(frame_size_val);
  log_i("Frame buffers: %s", frame_buffers_val);
  auto frame_buffers = atoi(frame_buffers_val);
  log_i("JPEG quality: %s", jpeg_quality_val);
  auto jpeg_quality = atoi(jpeg_quality_val);
  log_i("Frame rate: %s ms", frame_duration_val);

  camera_config.frame_size = frame_size;
  camera_config.fb_count = frame_buffers;
  camera_config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  camera_config.jpeg_quality = jpeg_quality;

  return cam.init(camera_config);
}

void start_rtsp_server()
{
  log_v("start_rtsp_server");
  camera_init_result = initialize_camera();
  if (camera_init_result != ESP_OK)
  {
    log_e("Failed to initialize camera: 0x%0xd. Type: %s, frame size: %s, frame buffers: %s, frame rate: %s ms, jpeg quality: %s", camera_init_result, camera_config_val, frame_size_val, frame_buffers_val, frame_duration_val, jpeg_quality_val);
    return;
  }

  log_i("Camera initialized");
  auto frame_rate = atol(frame_duration_val);
  camera_server = std::unique_ptr<rtsp_server>(new rtsp_server(cam, frame_rate, RTSP_PORT));
  // Add service to mDNS - rtsp
  MDNS.addService("rtsp", "tcp", 554);
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

  log_i("CPU Freq: %d Mhz", getCpuFrequencyMhz());
  log_i("Free heap: %d bytes", ESP.getFreeHeap());
  log_i("Starting " APP_TITLE "...");

  config_group_stream_settings.addItem(&config_camera_config);
  config_group_stream_settings.addItem(&config_frame_rate);
  config_group_stream_settings.addItem(&config_frame_size);
  config_group_stream_settings.addItem(&config_frame_buffers);
  config_group_stream_settings.addItem(&config_jpg_quality);
  iotWebConf.addParameterGroup(&config_group_stream_settings);
  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.setConfigSavedCallback(on_config_saved);
  iotWebConf.setWifiConnectionCallback(on_connected);
  iotWebConf.init();

  // Set up required URL handlers on the web server
  web_server.on("/", HTTP_GET, handle_root);
  web_server.on("/config", []
                { iotWebConf.handleConfig(); });
  web_server.on("/restart", HTTP_GET, handle_restart);
  // Camera snapshot
  web_server.on("/snapshot", handle_snapshot);

  // bootstrap
  web_server.on("/bootstrap.min.css", HTTP_GET, []()
                { stream_text_file(file_data_bootstrap_min_css, "text/css"); });

  web_server.onNotFound([]()
                        { iotWebConf.handleNotFound(); });

  ArduinoOTA
      .onStart([]()
               { log_w("Starting OTA update: %s", ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem"); })
      .onEnd([]()
             { log_w("OTA update done!"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { log_i("OTA Progress: %u%%\r", (progress / (total / 100))); })
      .onError([](ota_error_t error)
               {
      switch (error)
      {
      case OTA_AUTH_ERROR: log_e("OTA: Auth Failed"); break;
      case OTA_BEGIN_ERROR: log_e("OTA: Begin Failed"); break;
      case OTA_CONNECT_ERROR: log_e("OTA: Connect Failed"); break;
      case OTA_RECEIVE_ERROR: log_e("OTA: Receive Failed"); break;
      case OTA_END_ERROR: log_e("OTA: End Failed"); break;
      default: log_e("OTA error: %u", error);
      } });
  ArduinoOTA.setPassword(OTA_PASSWORD);

  // Start (OTA) Over The Air programming  when connected
  iotWebConf.setWifiConnectionCallback([]()
                                       { ArduinoOTA.begin(); });
}

void loop()
{
  iotWebConf.doLoop();
  ArduinoOTA.handle();

  if (camera_server)
    camera_server->doLoop();
}