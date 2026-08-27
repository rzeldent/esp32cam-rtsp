#pragma once

#include <micro_rtsp_source.h>
#include <esp_camera.h>
#include <map>

// Video source that captures JPEG frames using the esp32-camera driver.
// Each frame is copied out of the camera framebuffer and the framebuffer is
// returned immediately, so the camera keeps capturing while the copied frame
// is streamed (decoupling capture from transmission).
class micro_rtsp_camera : public micro_rtsp_source
{
public:
    micro_rtsp_camera();
    virtual ~micro_rtsp_camera();

    esp_err_t initialize(camera_config_t *camera_config);
    esp_err_t deinitialize();

    bool available() const { return jpeg_size_ > 0; }

    virtual void update_frame();

    virtual uint8_t *data() const { return jpeg_size_ > 0 ? jpeg_buffer_ : nullptr; }
    virtual size_t width() const { return frame_width_; }
    virtual size_t height() const { return frame_height_; }
    virtual size_t size() const { return jpeg_size_; }

private:
    // Lookup table mapping sensor PID (see camera_pid_t in sensor.h) to a human-readable model name.
    static const std::map<uint16_t, const char *> sensor_names_;

    // Copies the current camera framebuffer into jpeg_buffer_ and returns the
    // framebuffer to the camera driver.
    bool copy_frame_from_camera();

    // Ensures jpeg_buffer_ can hold at least capacity bytes (grows only).
    bool ensure_jpeg_buffer(size_t capacity);
    void free_jpeg_buffer();

    esp_err_t init_result_;
    camera_fb_t *fb_;            // transient: held only while copying a frame
    uint8_t *jpeg_buffer_;       // owned copy of the current JPEG frame
    size_t jpeg_buffer_capacity_;
    size_t jpeg_size_;
    size_t frame_width_;
    size_t frame_height_;
};