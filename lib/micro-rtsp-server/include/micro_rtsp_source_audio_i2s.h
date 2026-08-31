#pragma once

#include <micro_rtsp_source_audio.h>

#include <stdint.h>

// Captures audio from an I2S MEMS microphone, downsamples it to 8 kHz mono and encodes it to G.711 a-law so it can be streamed over RTP (payload 8).
//
// Example (Seeed Studio XIAO ESP32S3 Sense onboard microphone):
//   micro_rtsp_audio_i2s audio(17 /* bclk */, 42 /* ws */, 41 /* din */);
//   audio.begin();

class micro_rtsp_source_audio_i2s : public micro_rtsp_source_audio
{
public:
    // bclk_pin - I2S bit clock (BCK), ws_pin - word select (WS), data_pin - data in (DIN)
    micro_rtsp_source_audio_i2s(int8_t bclk_pin, int8_t ws_pin, int8_t data_pin);
    virtual ~micro_rtsp_source_audio_i2s();

    // Initialize the I2S peripheral. Call once before the server starts.
    bool begin();

    // Audio is only streamed when the I2S microphone initialized successfully.
    virtual bool available() const override { return initialized_; }

    virtual bool update();

    virtual const uint8_t *data() const { return alaw_buffer_; }
    virtual size_t size() const { return alaw_size_; }
    virtual uint32_t sample_rate() const { return 8000; }
    virtual uint8_t channels() const { return 1; }

private:
    static uint8_t linear_to_alaw(int16_t sample);

    int8_t bclk_pin_;
    int8_t ws_pin_;
    int8_t data_pin_;
    bool initialized_;

    // DMA buffer holding raw 16 bit PCM samples (one 20 ms chunk at 16 kHz)
    static constexpr size_t pcm_buffer_capacity_ = 512;
    int16_t pcm_buffer_[pcm_buffer_capacity_];

    // Downsampled + a-law encoded output
    static constexpr size_t alaw_capacity_ = 1024;
    uint8_t alaw_buffer_[alaw_capacity_];
    size_t alaw_size_;
};
