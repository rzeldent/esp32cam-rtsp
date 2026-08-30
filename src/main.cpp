#include <Arduino.h>
#include <esp_wifi.h>
#include <soc/rtc_cntl_reg.h>
#include <IotWebConf.h>
#include <string>
#include <esp_random.h>
#include <IotWebConfTParameter.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <libb64/cdecode.h>
#include <vector>
#include <lookup_camera_effect.h>
#include <lookup_camera_frame_size.h>
#include <lookup_camera_gainceiling.h>
#include <lookup_camera_wb_mode.h>
#include <format_duration.h>
#include <format_number.h>
#include <moustache.h>
#include <settings.h>

#include <micro_rtsp_server.h>
#include <micro_rtsp_source_video_camera.h>
#include <micro_rtsp_source_audio_i2s.h>

// HTML files
extern const char index_html_min_start[] asm("_binary_html_index_min_html_start");

auto param_group_camera = iotwebconf::ParameterGroup("camera", "Camera settings");
auto param_frame_duration = iotwebconf::Builder<iotwebconf::UIntTParameter<unsigned long>>("fd").label("Frame duration (ms)").defaultValue(DEFAULT_FRAME_DURATION).min(10).build();
auto param_frame_size = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(frame_sizes[0])>>("fs").label("Frame size").optionValues((const char *)&frame_sizes).optionNames((const char *)&frame_sizes).optionCount(sizeof(frame_sizes) / sizeof(frame_sizes[0])).nameLength(sizeof(frame_sizes[0])).defaultValue(DEFAULT_FRAME_SIZE).build();
auto param_jpg_quality = iotwebconf::Builder<iotwebconf::UIntTParameter<byte>>("q").label("JPG quality").defaultValue(DEFAULT_JPEG_QUALITY).min(1).max(100).build();
auto param_brightness = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("b").label("Brightness").defaultValue(DEFAULT_BRIGHTNESS).min(-2).max(2).build();
auto param_contrast = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("c").label("Contrast").defaultValue(DEFAULT_CONTRAST).min(-2).max(2).build();
auto param_saturation = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("s").label("Saturation").defaultValue(DEFAULT_SATURATION).min(-2).max(2).build();
auto param_special_effect = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_effects[0])>>("e").label("Effect").optionValues((const char *)&camera_effects).optionNames((const char *)&camera_effects).optionCount(sizeof(camera_effects) / sizeof(camera_effects[0])).nameLength(sizeof(camera_effects[0])).defaultValue(DEFAULT_EFFECT).build();
auto param_whitebal = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("wb").label("White balance").defaultValue(DEFAULT_WHITE_BALANCE).build();
auto param_awb_gain = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("awbg").label("Automatic white balance gain").defaultValue(DEFAULT_WHITE_BALANCE_GAIN).build();
auto param_wb_mode = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_wb_modes[0])>>("wbm").label("White balance mode").optionValues((const char *)&camera_wb_modes).optionNames((const char *)&camera_wb_modes).optionCount(sizeof(camera_wb_modes) / sizeof(camera_wb_modes[0])).nameLength(sizeof(camera_wb_modes[0])).defaultValue(DEFAULT_WHITE_BALANCE_MODE).build();
auto param_exposure_ctrl = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("ec").label("Exposure control").defaultValue(DEFAULT_EXPOSURE_CONTROL).build();
auto param_aec2 = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("aec2").label("Auto exposure (dsp)").defaultValue(DEFAULT_AEC2).build();
auto param_ae_level = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("ael").label("Auto Exposure level").defaultValue(DEFAULT_AE_LEVEL).min(-2).max(2).build();
auto param_aec_value = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("aecv").label("Manual exposure value").defaultValue(DEFAULT_AEC_VALUE).min(9).max(1200).build();
auto param_gain_ctrl = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("gc").label("Gain control").defaultValue(DEFAULT_GAIN_CONTROL).build();
auto param_agc_gain = iotwebconf::Builder<iotwebconf::IntTParameter<int>>("agcg").label("AGC gain").defaultValue(DEFAULT_AGC_GAIN).min(0).max(30).build();
auto param_gain_ceiling = iotwebconf::Builder<iotwebconf::SelectTParameter<sizeof(camera_gain_ceilings[0])>>("gcl").label("Auto Gain ceiling").optionValues((const char *)&camera_gain_ceilings).optionNames((const char *)&camera_gain_ceilings).optionCount(sizeof(camera_gain_ceilings) / sizeof(camera_gain_ceilings[0])).nameLength(sizeof(camera_gain_ceilings[0])).defaultValue(DEFAULT_GAIN_CEILING).build();
auto param_bpc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("bpc").label("Black pixel correct").defaultValue(DEFAULT_BPC).build();
auto param_wpc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("wpc").label("White pixel correct").defaultValue(DEFAULT_WPC).build();
auto param_raw_gma = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("rg").label("Gamma correct").defaultValue(DEFAULT_RAW_GAMMA).build();
auto param_lenc = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("lenc").label("Lens correction").defaultValue(DEFAULT_LENC).build();
auto param_hmirror = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("hm").label("Horizontal mirror").defaultValue(DEFAULT_HORIZONTAL_MIRROR).build();
auto param_vflip = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("vm").label("Vertical mirror").defaultValue(DEFAULT_VERTICAL_MIRROR).build();
auto param_dcw = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("dcw").label("Downsize enable").defaultValue(DEFAULT_DCW).build();
auto param_colorbar = iotwebconf::Builder<iotwebconf::CheckboxTParameter>("cb").label("Colorbar").defaultValue(DEFAULT_COLORBAR).build();

auto param_group_auth = iotwebconf::ParameterGroup("auth", "Authentication settings");
auto param_auth_user = iotwebconf::Builder<iotwebconf::TextTParameter<16>>("au").label("Username").defaultValue("").build();
auto param_auth_pass = iotwebconf::Builder<iotwebconf::PasswordTParameter<32>>("ap").label("Password").defaultValue("").build();

// DNS Server
DNSServer dnsServer;

// ESP32 Camera
micro_rtsp_source_video_camera camera;

#ifdef MIC_I2S_BCLK
// Optional audio: capture from the onboard I2S MEMS microphone and stream it
// as G.711 a-law together with the video (see boards/*.json for the pins).
micro_rtsp_source_audio_i2s audio(MIC_I2S_BCLK, MIC_I2S_WS, MIC_I2S_DIN);
micro_rtsp_server rtsp_server(DEFAULT_WWW_REALM, &camera, &audio);
#else
micro_rtsp_server rtsp_server(DEFAULT_WWW_REALM, &camera);
#endif

// Web server on port 80 for configuration and diagnostics. The RTSP server runs on port 554 (default) or the configured port.
WebServer web_server;

auto macAddress = String(ESP.getEfuseMac(), 16);
auto thingName = String(WIFI_SSID) + "-" + macAddress;
IotWebConf iotWebConf(thingName.c_str(), &dnsServer, &web_server, WIFI_PASSWORD, CONFIG_VERSION);

// Camera initialization result (ESP_FAIL until the camera is initialized)
esp_err_t camera_init_result = ESP_FAIL;

void handle_root()
{
  log_v("Handle root");
  // Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
    return;

  // Format hostname format: esp32-<mac address>.local
  auto hostname = "esp32-" + macAddress + ".local";

  // Wifi Modes
  const char *wifi_modes[] = {"NULL", "STA", "AP", "STA+AP"};
  auto ipv4 = WiFi.getMode() == WIFI_MODE_AP ? WiFi.softAPIP() : WiFi.localIP();

  auto initResult = esp_err_to_name(camera_init_result);
  if (initResult == nullptr)
    initResult = "Unknown reason";

  moustache_variable_t substitutions[] = {
      // Version / CPU
      {"AppTitle", APP_TITLE},
      {"AppVersion", APP_VERSION},
      {"BoardType", BOARD_NAME},
      {"ThingName", iotWebConf.getThingName()},
      {"SDKVersion", ESP.getSdkVersion()},
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
      {"NumRTSPSessions", String(rtsp_server.clients())},
      // Network
      {"HostName", hostname},
      {"MacAddress", macAddress},
      {"AccessPoint", WiFi.SSID()},
      {"SignalStrength", String(WiFi.RSSI())},
      {"WifiMode", wifi_modes[WiFi.getMode()]},
      {"IPv4", ipv4.toString()},
      {"NetworkState.ApMode", String(iotWebConf.getState() == iotwebconf::NetworkState::ApMode)},
      {"NetworkState.OnLine", String(iotWebConf.getState() == iotwebconf::NetworkState::OnLine)},
      // Camera
      {"FrameSize", String(param_frame_size.value())},
      {"FrameDuration", String(param_frame_duration.value())},
      {"FrameFrequency", String(1000.0 / param_frame_duration.value(), 1)},
      {"JpegQuality", String(param_jpg_quality.value())},
      {"CameraInitialized", String(camera_init_result == ESP_OK)},
      {"CameraInitResult", String(camera_init_result)},
      {"CameraInitResultText", initResult},
      // Settings
      {"Brightness", String(param_brightness.value())},
      {"Contrast", String(param_contrast.value())},
      {"Saturation", String(param_saturation.value())},
      {"SpecialEffect", String(param_special_effect.value())},
      {"WhiteBal", String(param_whitebal.value())},
      {"AwbGain", String(param_awb_gain.value())},
      {"WbMode", String(param_wb_mode.value())},
      {"ExposureCtrl", String(param_exposure_ctrl.value())},
      {"Aec2", String(param_aec2.value())},
      {"AeLevel", String(param_ae_level.value())},
      {"AecValue", String(param_aec_value.value())},
      {"GainCtrl", String(param_gain_ctrl.value())},
      {"AgcGain", String(param_agc_gain.value())},
      {"GainCeiling", String(param_gain_ceiling.value())},
      {"Bpc", String(param_bpc.value())},
      {"Wpc", String(param_wpc.value())},
      {"RawGma", String(param_raw_gma.value())},
      {"Lenc", String(param_lenc.value())},
      {"HMirror", String(param_hmirror.value())},
      {"VFlip", String(param_vflip.value())},
      {"Dcw", String(param_dcw.value())},
      {"ColorBar", String(param_colorbar.value())},
      // RTSP
      {"RtspPort", String(rtsp_server.get_rtsp_port())},
      {"AuthRequired", String(param_auth_user.value()[0] != '\0')},
      // OTA
      {"OtaSupported", String(esp_ota_get_next_update_partition(nullptr) != nullptr)}};

  web_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  auto html = moustache_render(index_html_min_start, substitutions);
  web_server.send(200, "text/html", html);
}

#ifdef FLASH_LED_GPIO
void handle_flash()
{
  log_v("handle_flash");
  // If no value present, use off, otherwise convert v to integer. Depends on analog resolution for max value
  auto v = web_server.hasArg("v") ? web_server.arg("v").toInt() : 0;
  // If conversion fails, v = 0
  analogWrite(FLASH_LED_GPIO, v);

  web_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  web_server.send(200);
}
#endif

void handle_snapshot()
{
  log_v("handle_snapshot");
  if (camera_init_result != ESP_OK)
  {
    web_server.send(404, "text/plain", "Camera is not initialized");
    return;
  }

  // Remove old images stored in the frame buffer
  auto frame_buffers = CAMERA_CONFIG_FB_COUNT;
  while (frame_buffers--)
    camera.update();

  auto fb_len = camera.size();
  auto fb = camera.data();
  if (fb == nullptr)
  {
    web_server.send(404, "text/plain", "Unable to obtain frame buffer from the camera");
    return;
  }

  web_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  web_server.setContentLength(fb_len);
  web_server.send(200, "image/jpeg", "");
  web_server.sendContent((const char *)fb, fb_len);
}

// Generate an RFC 2046 (5.1.1) compliant multipart boundary: 1-70 characters
// from DIGIT/ALPHA plus a few punctuation marks. Built from esp_random() so it
// is unique per stream connection and effectively never collides with the
// bytes of an encapsulated JPEG frame.
static std::string generate_stream_boundary()
{
  std::string boundary;
  boundary.reserve(32);
  static const char hex_digits[] = "0123456789abcdef";
  for (auto i = 0; i < 8; ++i) // 8 random words -> 32 hex chars
  {
    const uint32_t r = esp_random();
    for (auto j = 0; j < 4; ++j)
      boundary += hex_digits[(r >> (j * 4)) & 0xf];
  }
  return boundary;
}

void handle_stream()
{
  log_v("handle_stream");
  if (camera_init_result != ESP_OK)
  {
    web_server.send(404, "text/plain", "Camera is not initialized");
    return;
  }

  log_v("starting streaming");
  const std::string boundary = generate_stream_boundary();

  // Pace frames to the configured frame interval and service the RTSP server
  // and the web config/DNS/mDNS machinery between frames, so this handler
  // does not block the rest of the firmware while the connection is open.
  auto client = web_server.client();
  client.write(("HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: multipart/x-mixed-replace; boundary=" + boundary + "\r\n").c_str());

  const auto frame_interval = rtsp_server.get_frame_interval();
  unsigned long next_frame = 0;
  while (client.connected())
  {
    auto now = millis();
    if (now >= next_frame)
    {
      camera.update();
      if (camera.data() != nullptr)
      {
        client.write(("\r\n--" + boundary + "\r\n").c_str());
        client.write(("Content-Type: image/jpeg\r\nContent-Length: " + std::to_string(camera.size()) + "\r\n\r\n").c_str());
        client.write(camera.data(), camera.size());
      }
      next_frame = now + frame_interval;
    }

    yield(); // Yield to the RTOS so other tasks can run while waiting for the next frame
    // Keep RTSP streaming and the web config/DNS/mDNS loop alive while this HTTP connection is open.
    loop();
  }

  log_v("client disconnected");
  client.stop();
  log_v("stopped streaming");
}

void handle_restart()
{
  log_v("handle_restart");
  WiFi.disconnect(false, true);
  ESP.restart();
}

esp_err_t initialize_camera()
{
  log_v("initialize_camera");

  log_i("Frame size: %s", param_frame_size.value());
  auto frame_size = lookup_frame_size(param_frame_size.value());
  log_i("JPEG quality: %d", param_jpg_quality.value());
  auto jpeg_quality = param_jpg_quality.value();
  log_i("Frame duration: %d ms", param_frame_duration.value());

  // Set frame duration
  rtsp_server.set_frame_interval(param_frame_duration.value());
  // initialize the camera with the current configuration
  return camera.initialize(frame_size, jpeg_quality);
}

void update_camera_settings()
{
  // (Re)initialize the camera with the current configuration. This is called
  // at startup (after the config is loaded) and whenever the config is saved,
  // so frame size / JPEG quality / frame duration changes take effect
  // immediately instead of only after a reboot.
  if (camera_init_result == ESP_OK)
    esp_camera_deinit(); // re-init applies a new frame size / quality

  for (auto i = 0; i < 3; i++)
  {
    log_i("Initializing camera...");
    camera_init_result = initialize_camera();
    if (camera_init_result == ESP_OK)
      break;

    esp_camera_deinit();
    log_e("Failed to initialize camera. Error: 0x%04x. Frame size: %s, frame rate: %d ms, jpeg quality: %d", camera_init_result, param_frame_size.value(), param_frame_duration.value(), param_jpg_quality.value());
    delay(500);
  }

  auto camera = esp_camera_sensor_get();
  if (camera == nullptr)
  {
    log_e("Unable to get camera sensor");
    return;
  }

  camera->set_brightness(camera, param_brightness.value());
  camera->set_contrast(camera, param_contrast.value());
  camera->set_saturation(camera, param_saturation.value());
  camera->set_special_effect(camera, lookup_camera_effect(param_special_effect.value()));
  camera->set_whitebal(camera, param_whitebal.value());
  camera->set_awb_gain(camera, param_awb_gain.value());
  camera->set_wb_mode(camera, lookup_camera_wb_mode(param_wb_mode.value()));
  camera->set_exposure_ctrl(camera, param_exposure_ctrl.value());
  camera->set_aec2(camera, param_aec2.value());
  camera->set_ae_level(camera, param_ae_level.value());
  camera->set_aec_value(camera, param_aec_value.value());
  camera->set_gain_ctrl(camera, param_gain_ctrl.value());
  camera->set_agc_gain(camera, param_agc_gain.value());
  camera->set_gainceiling(camera, lookup_camera_gainceiling(param_gain_ceiling.value()));
  camera->set_bpc(camera, param_bpc.value());
  camera->set_wpc(camera, param_wpc.value());
  camera->set_raw_gma(camera, param_raw_gma.value());
  camera->set_lenc(camera, param_lenc.value());
  camera->set_hmirror(camera, param_hmirror.value());
  camera->set_vflip(camera, param_vflip.value());
  camera->set_dcw(camera, param_dcw.value());
  camera->set_colorbar(camera, param_colorbar.value());
}

void start_rtsp_server()
{
  log_v("start_rtsp_server");

  rtsp_server.begin();
  // Add RTSP service to mDNS
  // HTTP is already set by iotWebConf
  MDNS.addService("rtsp", "tcp", rtsp_server.get_rtsp_port());
}

void on_connected()
{
  log_v("on_connected");

  // Start the RTSP Server if initialized
  if (camera_init_result == ESP_OK)
    start_rtsp_server();
  else
    log_e("Not starting RTSP server: camera not initialized");
}

void on_config_saved()
{
  log_v("on_config_saved");
  update_camera_settings();
  // Apply the RTSP authentication credentials immediately, so a changed
  // username/password takes effect for new clients without a reboot.
  // Existing sessions keep their previous credentials; only new connections
  // are affected.
  rtsp_server.set_credentials(param_auth_user.value(), param_auth_pass.value());
}

bool is_authenticated()
{
  // If no username is configured, authentication is disabled.
  if (param_auth_user.value()[0] == '\0')
    return true;

  // If the client has already authenticated, return true.
  if (web_server.authenticate(param_auth_user.value(), param_auth_pass.value()))
    return true;

  // Otherwise, request authentication.
  web_server.requestAuthentication(BASIC_AUTH, DEFAULT_WWW_REALM);
  return false;
}

// Pure OTA_PASSWORD check (no response is sent, so it is safe to call from the
// streaming upload handler, which cannot send a response mid-body). Returns true
// when the "Authorization: Basic base64(user:password)" header's password equals
// OTA_PASSWORD (any username is accepted); an empty OTA_PASSWORD disables auth.
bool ota_authorized()
{
  if (OTA_PASSWORD[0] == '\0')
    return true;

  auto auth = web_server.header("Authorization");
  if (auth.startsWith("Basic "))
  {
    auto token = auth.substring(6);
    token.trim();

    const auto capacity = base64_decode_expected_len(token.length()) + 1;
    std::vector<char> decoded(capacity);
    const auto len = base64_decode_chars(token.c_str(), static_cast<int>(token.length()), decoded.data());
    if (len > 0)
    {
      const String credentials(decoded.data(), len); // "user:password"
      const auto colon = credentials.indexOf(':');
      const auto password = colon >= 0 ? credentials.substring(colon + 1) : credentials;
      if (password == OTA_PASSWORD)
        return true;
    }
  }
  return false;
}

// --- Over-the-air (OTA) firmware updates ---------------------------------------
//
// Two mechanisms are provided:
//   1. ArduinoOTA (espota protocol, port 3232) - used by PlatformIO
//      (`pio run -t upload --upload-protocol espota`) and the Arduino IDE.
//   2. A web updater at /update - upload firmware.bin through the browser. The
//      upload form is embedded in the root page and the endpoint is protected by
//      OTA_PASSWORD (include/settings.h), independent of the RTSP credentials.
//
// Both require a partition table with two app slots (see partitions/ota.csv);
// 8MB/16MB boards already ship one.

static String ota_error = "";
static bool ota_authenticated = false;

void setup_ota()
{
  log_v("setup_ota");

  const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(nullptr);
  if (ota_partition == nullptr)
    log_w("OTA NOT available: single app slot - re-flash this build once over USB to install the OTA partition table (partitions/ota.csv)");
  else
    log_i("OTA supported: next OTA partition '%s' @ 0x%x, size 0x%x", ota_partition->label, ota_partition->address, ota_partition->size);

  // Network OTA via the espota protocol (port 3232), which allows PlatformIO / Arduino IDE to push new firmware over WiFi.
  ArduinoOTA.setHostname(thingName.c_str());
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA
      .onStart([]()
               { 
      // Free memory for the update: stop the RTSP server and release the camera frame buffers, so Update.write() has all the heap it needs
      // On a successful update the device reboots; onError() restores these.
      rtsp_server.end();
      if (camera_init_result == ESP_OK)
      {
        esp_camera_deinit();
        camera_init_result = ESP_FAIL;
      } })
      .onEnd([]()
             { log_w("OTA update finished"); })
      .onProgress([](unsigned int progress, unsigned int total)
                  { log_i("OTA progress: %u%%", (progress * 100) / total); })
      .onError([](ota_error_t error)
               {
      switch (error)
      {
      case OTA_AUTH_ERROR: log_e("OTA: authentication failed"); break;
      case OTA_BEGIN_ERROR: log_e("OTA: begin failed - %s", Update.errorString()); break;
      case OTA_CONNECT_ERROR: log_e("OTA: connect-back to the client FAILED (check the PC firewall for the host_port)"); break;
      case OTA_RECEIVE_ERROR: log_e("OTA: receive failed"); break;
      case OTA_END_ERROR: log_e("OTA: end failed - %s", Update.errorString()); break;
      default: log_e("OTA error: %u", error);
      }
      // Restore the camera and RTSP server after a failed update (on success the
      // device reboots, so no restore is needed there).
      update_camera_settings();
      if (iotWebConf.getState() == iotwebconf::NetworkState::OnLine)
        start_rtsp_server(); });

  ArduinoOTA.begin();
  log_i("ArduinoOTA (espota) service started");
}

// Post-upload response. Runs after the multipart body has been fully received.
void handle_update_done()
{
  log_v("handle_update_done");

  // Auth is only recorded at UPLOAD_FILE_START; answer 401 here (after the body
  // has been consumed) when the OTA_PASSWORD credentials were missing.
  if (!ota_authenticated)
  {
    web_server.requestAuthentication(BASIC_AUTH, DEFAULT_WWW_REALM);
    return;
  }

  if (ota_error.length() > 0 || Update.hasError())
  {
    auto error = ota_error.length() > 0 ? ota_error.c_str() : Update.errorString();
    log_e("OTA update failed: %s", error);
    // Bring the camera and RTSP server back up after a failed update.
    update_camera_settings();
    if (iotWebConf.getState() == iotwebconf::NetworkState::OnLine)
      start_rtsp_server();

    web_server.send(200, "text/plain", String("Update failed: ") + error);
    return;
  }

  log_w("OTA update success (%u bytes), rebooting...", Update.size());
  web_server.send(200, "text/plain", "Update success! Rebooting...");
  web_server.client().stop();
  delay(100);
  ESP.restart();
}

// Streams the uploaded firmware into the Update (esp_ota) machinery.
void handle_update_upload()
{
  // Reference, not copy: HTTPUpload holds a large buffer and web_server.upload()
  // dereferences a pointer that can be null, so copying would crash (LoadProhibited).
  HTTPUpload &upload = web_server.upload();
  if (upload.status == UPLOAD_FILE_START)
  {
    log_w("OTA upload start: %s (%u bytes)", upload.filename.c_str(), upload.totalSize);
    ota_error = "";

    // Authenticate once at the start. A 401 cannot be sent here while the body is
    // still streaming (it corrupts the WebServer upload state), so an unauthorized
    // upload is simply not written; handle_update_done() sends the 401 afterwards.
    ota_authenticated = ota_authorized();
    if (!ota_authenticated)
    {
      log_e("OTA upload rejected: authentication required");
      return;
    }

    if (!upload.filename.endsWith(".bin"))
    {
      ota_error = "Only .bin firmware files are supported";
      log_e("%s", ota_error.c_str());
      return;
    }

    // Free memory and dedicate the Wi-Fi path to the upload: stop the RTSP server and release the camera frame buffers.
    rtsp_server.end();
    if (camera_init_result == ESP_OK)
    {
      esp_camera_deinit();
      camera_init_result = ESP_FAIL;
    }

    // UPDATE_SIZE_UNKNOWN + end(true) accepts the actually received size.
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      ota_error = String("Update.begin failed: ") + Update.errorString();
      log_e("%s", ota_error.c_str());
    }
  }
  else if (ota_authenticated && upload.status == UPLOAD_FILE_WRITE && ota_error.length() == 0)
  {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      ota_error = String("Update.write failed: ") + Update.errorString();
      log_e("%s", ota_error.c_str());
    }
  }
  else if (ota_authenticated && upload.status == UPLOAD_FILE_END && ota_error.length() == 0)
  {
    if (!Update.end(true))
    {
      ota_error = String("Update.end failed: ") + Update.errorString();
      log_e("%s", ota_error.c_str());
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED)
  {
    log_w("OTA upload aborted");
    Update.abort();
  }
}

void setup()
{
  // Disable brownout
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
#ifdef CAMERA_POWER_GPIO
  pinMode(CAMERA_POWER_GPIO, OUTPUT);
  digitalWrite(CAMERA_POWER_GPIO, CAMERA_POWER_ON_LEVEL);
#endif

#ifdef USER_LED_GPIO
  pinMode(USER_LED_GPIO, OUTPUT);
  digitalWrite(USER_LED_GPIO, !USER_LED_ON_LEVEL);
#endif

#ifdef FLASH_LED_GPIO
  pinMode(FLASH_LED_GPIO, OUTPUT);
  // Set resolution to 8 bits
  analogWriteResolution(8);
  // Turn flash led off
  analogWrite(FLASH_LED_GPIO, 0);
#endif

#ifdef ARDUINO_USB_CDC_ON_BOOT
  // Delay for USB to connect/settle
  delay(5000);
#endif

  log_i("Core debug level: %d", CORE_DEBUG_LEVEL);
  log_i("CPU Freq: %d Mhz, %d core(s)", getCpuFrequencyMhz(), ESP.getChipCores());
  log_i("Free heap: %d bytes", ESP.getFreeHeap());
  log_i("SDK version: %s", ESP.getSdkVersion());
  log_i("Board: %s", BOARD_NAME);
  log_i("Starting " APP_TITLE "...");

  if (CAMERA_CONFIG_FB_LOCATION == CAMERA_FB_IN_PSRAM && !psramInit())
    log_e("Failed to initialize PSRAM");

#ifdef MIC_I2S_BCLK
  if (!audio.begin())
    log_e("Failed to initialize the I2S microphone");
#endif

  WiFi.setSleep(false);

#ifdef MICRO_RTSP_ENABLE_SRTP
  // Stream video over Secure RTP (RFC 3711), negotiated with "a=crypto"
  // (RFC 4568) in the DESCRIBE/SETUP replies.
  server.set_srtp(true);
  log_i("SRTP enabled");
#endif

  param_group_camera.addItem(&param_frame_duration);
  param_group_camera.addItem(&param_frame_size);
  param_group_camera.addItem(&param_jpg_quality);
  param_group_camera.addItem(&param_brightness);
  param_group_camera.addItem(&param_contrast);
  param_group_camera.addItem(&param_saturation);
  param_group_camera.addItem(&param_special_effect);
  param_group_camera.addItem(&param_whitebal);
  param_group_camera.addItem(&param_awb_gain);
  param_group_camera.addItem(&param_wb_mode);
  param_group_camera.addItem(&param_exposure_ctrl);
  param_group_camera.addItem(&param_aec2);
  param_group_camera.addItem(&param_ae_level);
  param_group_camera.addItem(&param_aec_value);
  param_group_camera.addItem(&param_gain_ctrl);
  param_group_camera.addItem(&param_agc_gain);
  param_group_camera.addItem(&param_gain_ceiling);
  param_group_camera.addItem(&param_bpc);
  param_group_camera.addItem(&param_wpc);
  param_group_camera.addItem(&param_raw_gma);
  param_group_camera.addItem(&param_lenc);
  param_group_camera.addItem(&param_hmirror);
  param_group_camera.addItem(&param_vflip);
  param_group_camera.addItem(&param_dcw);
  param_group_camera.addItem(&param_colorbar);
  iotWebConf.addParameterGroup(&param_group_camera);

  // RTSP Basic authentication credentials (RFC 2617), also used to protect /config.
  param_group_auth.addItem(&param_auth_user);
  param_group_auth.addItem(&param_auth_pass);
  iotWebConf.addParameterGroup(&param_group_auth);

  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.setConfigSavedCallback(on_config_saved);
  iotWebConf.setWifiConnectionCallback(on_connected);
#ifdef USER_LED_GPIO
  iotWebConf.setStatusPin(USER_LED_GPIO, USER_LED_ON_LEVEL);
#endif
  iotWebConf.init();

  // Set the time servers
  configTime(0, 0, NTP_SERVERS);

  // Initialize the camera with the now-loaded configuration and apply the
  // sensor settings. update_camera_settings() is also called on config save,
  // so a changed frame size / quality takes effect immediately.
  update_camera_settings();

  // Apply RTSP Basic authentication credentials (RFC 2617); empty username disables it.
  rtsp_server.set_credentials(param_auth_user.value(), param_auth_pass.value());

  // Set up required URL handlers on the web server
  web_server.on("/", HTTP_GET, handle_root);
  web_server.on("/config", []()
                {
                  // IotWebConf handles the captive portal and config page, including authentication!
                  // username: admin, password: AP password you set in the config portal
                  // Registered without a method so BOTH GET (open the page) and
                  // POST (form submit -> iotSave) are handled.
                    iotWebConf.handleConfig(); });
  // Camera snapshot
  web_server.on("/snapshot", HTTP_GET, []()
                {
                  if (is_authenticated())
                    handle_snapshot(); });
  // Camera stream
  web_server.on("/stream", HTTP_GET, []()
                {
                  if (is_authenticated())
                    handle_stream(); });
#ifdef FLASH_LED_GPIO
  // Flash led
  web_server.on("/flash", HTTP_GET, []()
                {
                  if (is_authenticated())
                    handle_flash(); });
#endif
  // ESP restart
  web_server.on("/restart", HTTP_GET, []()
                {
                  if (is_authenticated())
                    handle_restart(); });
  // Firmware update (OTA): the upload form is embedded in the root page; /update
  // accepts the firmware.bin upload (multipart/form-data). It is protected by
  // OTA_PASSWORD (include/settings.h), independent of the RTSP credentials.
  web_server.on("/update", HTTP_GET, []()
                { 
                  web_server.sendHeader("Location", "/");
                  web_server.send(302, "text/plain", ""); });
  web_server.on("/update", HTTP_POST, handle_update_done, handle_update_upload);
  web_server.onNotFound([]()
                        { iotWebConf.handleNotFound(); });

  // Start the ArduinoOTA (espota) service for network firmware uploads.
  setup_ota();
}

void loop()
{
  iotWebConf.doLoop();
  rtsp_server.loop();
  ArduinoOTA.handle();

  // OTA diagnostics: once an update is running, log progress + heap so the
  // serial monitor shows whether the firmware stream is actually arriving.
  // (Nothing logged here means Update.begin() never started - see onError.)
  static unsigned long last_ota_log = 0;
  if (Update.isRunning() && millis() - last_ota_log >= 2000)
  {
    last_ota_log = millis();
    log_i("OTA receiving: %u / %u bytes, free heap %u", Update.progress(), Update.size(), ESP.getFreeHeap());
  }
}
