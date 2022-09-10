#include <Arduino.h>
#include <soc/rtc_cntl_reg.h>
#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include <OV2640.h>
#include <ESPmDNS.h>
#include <rtsp_server.h>
#include <frame_size.h>
#include <camera_config.h>
#include <format_duration.h>
#include <format_si.h>
#include <SPIFFS.h>
#include <template_render.h>
#include <settings.h>

char camera_config_val[sizeof(camera_config_entry)];
char frame_duration_val[6];
char frame_size_val[sizeof(frame_size_entry_t)];
char jpeg_quality_val[4];

auto config_group_stream_settings = iotwebconf::ParameterGroup("settings", "Streaming settings");
auto config_camera_config = iotwebconf::SelectParameter("Camera config", "config", camera_config_val, sizeof(camera_config_val), (const char *)camera_configs, (const char *)camera_configs, sizeof(camera_configs) / sizeof(camera_configs[0]), sizeof(camera_configs[0]), DEFAULT_CAMERA_CONFIG);
auto config_frame_rate = iotwebconf::NumberParameter("Frame duration (ms)", "fd", frame_duration_val, sizeof(frame_duration_val), DEFAULT_FRAMEDURATION, nullptr, "min=\"10\"");
auto config_frame_size = iotwebconf::SelectParameter("Frame size", "fs", frame_size_val, sizeof(frame_size_val), (const char *)frame_sizes, (const char *)frame_sizes, sizeof(frame_sizes) / sizeof(frame_sizes[0]), sizeof(frame_sizes[0]), DEFAULT_FRAMESIZE);
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
// Check if camera is initialized
bool camera_initialized = false;

void stream_file(const char *spiffs_file, const char *mime_type)
{
  // Cache for 86400 seconds (one day)
  web_server.sendHeader("Cache-Control", "max-age=86400");
  auto file = SPIFFS.open(spiffs_file);
  web_server.streamFile(file, mime_type);
  file.close();
}

void handle_root()
{
  log_v("Handle root");
  // Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
    return;

  const template_variable_t substitutions[] = {
      {"AppTitle", APP_TITLE},
      {"AppVersion", APP_VERSION},
      {"ThingName", iotWebConf.getThingName()},
      {"ChipModel", ESP.getChipModel()},
      {"CpuFreqMHz", String(ESP.getCpuFreqMHz())},
      {"MacAddress", WiFi.macAddress()},
      {"IpV4", WiFi.localIP().toString()},
      {"IpV6", WiFi.localIPv6().toString()},
      {"CameraType", camera_config_val},
      {"FrameSize", frame_size_val},
      {"FrameDuration", frame_duration_val},
      {"FrameFrequency", String(1000.0 / atol(frame_duration_val), 1)},
      {"JpegQuality", jpeg_quality_val},
      {"Uptime", String(format_duration(millis() / 1000))},
      {"FreeHeap", format_si(ESP.getFreeHeap())},
      {"MaxAllocHeap", format_si(ESP.getMaxAllocHeap())},
      {"RtspPort", String(RTSP_PORT)},
      {"ConfigChanged", String(config_changed)},
      {"CameraInitialized", String(camera_initialized)}};

  web_server.sendHeader("Cache-Control", "no-cache");
  auto file = SPIFFS.open("/index.html");
  auto html = template_render(file.readString(), substitutions);
  file.close();
  web_server.send(200, "text/html", html);
}

void handle_restart()
{
  log_v("Handle restart");
  //  if (!config_changed)
  //  {
  // Redirect to root page.
  //    web_server.sendHeader("Location", "/", true);
  //    web_server.send(302, "text/plain", "");
  //    return;
  //  }

  const template_variable_t substitutions[] = {
      {"AppTitle", APP_TITLE},
      {"AppVersion", APP_VERSION},
      {"ThingName", iotWebConf.getThingName()}};

  web_server.sendHeader("Cache-Control", "no-cache");
  auto file = SPIFFS.open("/restart.html");
  auto html = template_render(file.readString(), substitutions);
  file.close();
  web_server.send(200, "text/html", html);
  log_v("Restarting... Press refresh to connect again");
  sleep(1000);
  // ESP.restart();
}

void on_config_saved()
{
  log_v("on_config_saved");
  config_changed = true;
}

bool initialize_camera()
{
  log_v("initialize_camera");
  log_i("Camera config: %s", camera_config_val);
  auto camera_config = lookup_camera_config(camera_config_val);
  log_i("Frame size: %s", frame_size_val);
  auto frame_size = lookup_frame_size(frame_size_val);
  log_i("JPEG quality: %s", jpeg_quality_val);
  auto jpeg_quality = atoi(jpeg_quality_val);
  log_i("Frame rate: %s ms", frame_duration_val);

  camera_config.frame_size = frame_size;
  camera_config.jpeg_quality = jpeg_quality;
  return cam.init(camera_config) == ESP_OK;
}

void start_rtsp_server()
{
  log_v("start_rtsp_server");
  camera_initialized = initialize_camera();
  if (!camera_initialized)
  {
    log_e("Failed to initialize camera. Type: %s, frame size: %s, frame rate: %s ms, jpeg quality: %s", camera_config_val, frame_size_val, frame_duration_val, jpeg_quality_val);
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

  if (!SPIFFS.begin())
    log_e("Error while mounting SPIFFS. Please upload the filesystem");

  config_group_stream_settings.addItem(&config_camera_config);
  config_group_stream_settings.addItem(&config_frame_rate);
  config_group_stream_settings.addItem(&config_frame_size);
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

  // bootstrap
  web_server.on("/bootstrap.min.css", HTTP_GET, []()
                { stream_file("/bootstrap.min.css", "text/css"); });

  web_server.onNotFound([]()
                        { iotWebConf.handleNotFound(); });

  // Set DNS to thing name
  MDNS.begin(iotWebConf.getThingName());
  // Add service to mDNS - http
  MDNS.addService("http", "tcp", 80);
}

void loop()
{
  iotWebConf.doLoop();

  if (camera_server)
    camera_server->doLoop();
}