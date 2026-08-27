#include <esp32-hal-log.h>

#include <cstring>
#include <esp_heap_caps.h>

#include "micro_rtsp_camera.h"

// Map of sensor PID to model name (see camera_pid_t in sensor.h).
const std::map<uint16_t, const char *> micro_rtsp_camera::sensor_names_ = {
    {OV9650_PID, "OmniVision OV9650"},
    {OV7725_PID, "OmniVision OV7725"},
    {OV2640_PID, "OmniVision OV2640"},
    {OV3660_PID, "OmniVision OV3660"},
    {OV5640_PID, "OmniVision OV5640"},
    {OV7670_PID, "OmniVision OV7670"},
    {NT99141_PID, "NT99141"},
    {GC2145_PID, "GalaxyCore GC2145"},
    {GC032A_PID, "GalaxyCore GC032A"},
    {GC0308_PID, "GalaxyCore GC0308"},
    {BF3005_PID, "BF3005"},
    {BF20A6_PID, "BF20A6"},
    {SC101IOT_PID, "SmartSens SC101IOT"},
    {SC030IOT_PID, "SmartSens SC030IOT"},
    {SC031GS_PID, "SmartSens SC031GS"},
};

micro_rtsp_camera::micro_rtsp_camera()
: init_result_(ESP_FAIL),
  fb_(nullptr),
  jpeg_buffer_(nullptr),
  jpeg_buffer_capacity_(0),
  jpeg_size_(0),
  frame_width_(0),
  frame_height_(0)
{
}

micro_rtsp_camera::~micro_rtsp_camera()
{
    deinitialize();
}

esp_err_t micro_rtsp_camera::initialize(camera_config_t *camera_config)
{
    log_v("camera_config={.pin_pwdn:%u,.pin_reset:%u,.pin_xclk:%u,.pin_sccb_sda:%u,.pin_sccb_scl:%u,.pin_d7:%u,.pin_d6:%u,.pin_d5:%u,.pin_d4:%u,.pin_d3:%u,.pin_d2:%u,.pin_d1:%u,.pin_d0:%u,.pin_vsync:%u,.pin_href:%u,.pin_pclk:%u,.xclk_freq_hz:%d,.ledc_timer:%u,ledc_channel:%u,.pixel_format:%d,.frame_size:%d,.jpeg_quality:%d,.fb_count:%d,.fb_location%d,.grab_mode:%d,sccb_i2c_port:%d}", camera_config->pin_pwdn, camera_config->pin_reset, camera_config->pin_xclk, camera_config->pin_sccb_sda, camera_config->pin_sccb_scl, camera_config->pin_d7, camera_config->pin_d6, camera_config->pin_d5, camera_config->pin_d4, camera_config->pin_d3, camera_config->pin_d2, camera_config->pin_d1, camera_config->pin_d0, camera_config->pin_vsync, camera_config->pin_href, camera_config->pin_pclk, camera_config->xclk_freq_hz, camera_config->ledc_timer, camera_config->ledc_channel, camera_config->pixel_format, camera_config->frame_size, camera_config->jpeg_quality, camera_config->fb_count, camera_config->fb_location, camera_config->grab_mode, camera_config->sccb_i2c_port);

    init_result_ = esp_camera_init(camera_config);
    if (init_result_ == ESP_OK)
    {
        auto sensor = esp_camera_sensor_get();
        auto it = sensor_names_.find(sensor->id.PID);
        const char *model = (it != sensor_names_.end()) ? it->second : "Unknown";
        log_i("Found camera sensor: model=%s, PID=0x%04x, MID=0x%04x, VER=0x%02x", model, sensor->id.PID, (sensor->id.MIDH << 8) | sensor->id.MIDL, sensor->id.VER);
        update_frame();
    }
    else
        log_e("Camera initialization failed: 0x%02x", init_result_);

    return init_result_;
}

esp_err_t micro_rtsp_camera::deinitialize()
{
    if (fb_)
    {
        esp_camera_fb_return(fb_);
        fb_ = nullptr;
    }

    free_jpeg_buffer();

    return init_result_ == ESP_OK ? esp_camera_deinit() : ESP_OK;
}

void micro_rtsp_camera::update_frame()
{
    // Return any previous framebuffer (defensive; it is normally already
    // returned right after the copy in copy_frame_from_camera()).
    if (fb_)
    {
        esp_camera_fb_return(fb_);
        fb_ = nullptr;
    }

    fb_ = esp_camera_fb_get();
    if (fb_ == nullptr)
    {
        // No new frame available; mark the previous copy as consumed so the
        // caller does not stream a stale frame.
        jpeg_size_ = 0;
        return;
    }

    copy_frame_from_camera();
}

bool micro_rtsp_camera::copy_frame_from_camera()
{
    if (fb_ == nullptr)
        return false;

    if (!ensure_jpeg_buffer(fb_->len))
    {
        log_e("Failed to allocate %u bytes for the JPEG copy buffer", (unsigned)fb_->len);
        esp_camera_fb_return(fb_);
        fb_ = nullptr;
        jpeg_size_ = 0;
        return false;
    }

    memcpy(jpeg_buffer_, fb_->buf, fb_->len);
    jpeg_size_ = fb_->len;
    frame_width_ = fb_->width;
    frame_height_ = fb_->height;

    // Return the framebuffer immediately so the camera keeps capturing into it
    // while this copy is streamed out (true double buffering).
    esp_camera_fb_return(fb_);
    fb_ = nullptr;

    return true;
}

bool micro_rtsp_camera::ensure_jpeg_buffer(size_t capacity)
{
    if (jpeg_buffer_ != nullptr && jpeg_buffer_capacity_ >= capacity)
        return true;

    // Prefer PSRAM (plenty for a full frame), falling back to internal RAM for boards without PSRAM.
    uint8_t *buffer = (uint8_t *)heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM);
    if (buffer == nullptr)
        buffer = (uint8_t *)heap_caps_malloc(capacity, MALLOC_CAP_8BIT);
    
    if (buffer == nullptr)
        return false;

    free_jpeg_buffer();
    jpeg_buffer_ = buffer;
    jpeg_buffer_capacity_ = capacity;
    return true;
}

void micro_rtsp_camera::free_jpeg_buffer()
{
    if (jpeg_buffer_ != nullptr)
    {
        heap_caps_free(jpeg_buffer_);
        jpeg_buffer_ = nullptr;
        jpeg_buffer_capacity_ = 0;
    }
}