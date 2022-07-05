#include <Arduino.h>
#include <soc/rtc_cntl_reg.h>
#include <IotWebConf.h>
#include <IotWebConfTParameter.h>
#include <OV2640.h>
#include <ESPmDNS.h>
#include <rtsp_server.h>
#include <frame_size.h>
#include <camera_config.h>
#include <settings.h>

char camera_config_val[sizeof(camera_config_entry)];
char frame_rate_val[6];
char frame_size_val[sizeof(frame_size_entry_t)];
char jpeg_quality_val[4];

auto config_group_stream_settings = iotwebconf::ParameterGroup("settings", "Streaming settings");
auto config_camera_config = iotwebconf::SelectParameter("Camera config", "config", camera_config_val, sizeof(camera_config_val), (const char *)camera_configs, (const char *)camera_configs, sizeof(camera_configs) / sizeof(camera_configs[0]), sizeof(camera_configs[0]), DEFAULT_CAMERA_CONFIG);
auto config_frame_rate = iotwebconf::NumberParameter("Frame rate (ms)", "fr", frame_rate_val, sizeof(frame_rate_val), DEFAULT_FRAMERATE);
auto config_frame_size = iotwebconf::SelectParameter("Frame size", "fs", frame_size_val, sizeof(frame_size_val), (const char *)frame_sizes, (const char *)frame_sizes, sizeof(frame_sizes) / sizeof(frame_sizes[0]), sizeof(frame_sizes[0]), DEFAULT_FRAMESIZE);
auto config_jpg_quality = iotwebconf::NumberParameter("JPEG quality", "q", jpeg_quality_val, sizeof(jpeg_quality_val), DEFAULT_JPEG_QUALITY);

// Camera
OV2640 cam;
// DNS Server
DNSServer dnsServer;
// RTSP Server
std::unique_ptr<rtsp_server> camera_server;
// Web server
WebServer web_server(80);
IotWebConf iotWebConf(WIFI_SSID, &dnsServer, &web_server, WIFI_PASSWORD, CONFIG_VERSION);

bool config_changed = false;

void handle_root()
{
  log_v("Handle root");
  // Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
    return;

  auto url = "rtsp://" + String(iotWebConf.getThingName()) + ".local:" + String(RTSP_PORT) + "/mjpeg/1";

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
  html += "<li>IPv4 address: " + WiFi.localIP().toString() + "</li>";
  html += "<li>IPv6 address: " + WiFi.localIPv6().toString() + "</li>";
  html += "</ul>";
  html += "<h3>Settings</h3>";
  html += "<ul>";
  html += "<li>Camera type: " + String(camera_config_val) + "</li>";
  html += "<li>Frame size: " + String(frame_size_val) + "</li>";
  html += "<li>Frame rate: " + String(frame_rate_val) + " ms</li>";
  html += "<li>JPEG quality: " + String(jpeg_quality_val) + " (0-100)</li>";
  html += "</ul>";
  html += "<br/>camera stream: <a href=\"" + url + "\">" + url + "</a>";
  html += "<br />";
  html += "<br/>Go to <a href=\"config\">configure page</a> to change settings.";
  if (config_changed)
  {
    html += "<br />";
    html += "<br/><h3 style=\"color:red\">Configuration has changed. Please <a href=\"restart\">restart</a> the device.</h3>";
  }

  html += "</body></html>";
  web_server.send(200, "text/html", html);
}

void handle_restart()
{
  log_v("Handle restart");
  if (config_changed)
  {
    String html;
    html += "<h2>Restarting...</h2>";
    html += "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
    html += "<title>" APP_TITLE " v" APP_VERSION "</title></head>";
    html += "<body>";
    web_server.send(200, "text/html", html);
    log_v("Restarting...");
    sleep(250);
    ESP.restart();
  }
  else
  {
    // Redirect to root page.
    web_server.sendHeader("Location", "/", true);
    web_server.send(302, "text/plain", "");
  }
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
  log_i("Framerate: %s ms", frame_rate_val);

  camera_config.frame_size = frame_size;
  camera_config.jpeg_quality = jpeg_quality;
  return cam.init(camera_config) == ESP_OK;
}

void start_rtsp_server()
{
  log_v("start_rtsp_server");
  if (!initialize_camera())
  {
    log_e("Failed to initialize camera. Type: %s, frame size: %s, frame rate: %s ms, jpeg quality: %s", camera_config_val, frame_size_val, frame_rate_val, jpeg_quality_val);
    return;
  }
  auto frame_rate = atol(frame_rate_val);
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

  log_i("CPU Freq = %d Mhz", getCpuFrequencyMhz());
  log_i("Free heap: %d bytes", ESP.getFreeHeap());
  log_i("Starting " APP_TITLE "...");

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