#include <esp32-hal-log.h>
#include <driver/i2s.h>

#include "micro_rtsp_source_audio_i2s.h"

// Captures 16 kHz mono 16-bit PCM and downsamples it to 8 kHz before encoding to G.711 a-law (one byte per sample).
static constexpr uint32_t i2s_sample_rate = 16000;
static constexpr uint32_t output_sample_rate = 8000;

micro_rtsp_source_audio_i2s::micro_rtsp_source_audio_i2s(int8_t bclk_pin, int8_t ws_pin, int8_t data_pin)
    : bclk_pin_(bclk_pin), ws_pin_(ws_pin), data_pin_(data_pin), initialized_(false), alaw_size_(0)
{
}

micro_rtsp_source_audio_i2s::~micro_rtsp_source_audio_i2s()
{
    if (initialized_)
    {
        i2s_driver_uninstall(I2S_NUM_0);
        initialized_ = false;
    }
}

bool micro_rtsp_source_audio_i2s::begin()
{
    if (initialized_)
        return true;

    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2s_config.sample_rate = i2s_sample_rate;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_config.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 8;
    i2s_config.dma_buf_len = 64;
    i2s_config.use_apll = false;
    i2s_config.tx_desc_auto_clear = false;
    i2s_config.fixed_mclk = 0;

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = bclk_pin_;
    pin_config.ws_io_num = ws_pin_;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num = data_pin_;

    auto err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, nullptr);
    if (err != ESP_OK)
    {
        log_e("I2S driver install failed: 0x%x", err);
        return false;
    }

    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK)
    {
        log_e("I2S set pin failed: 0x%x", err);
        i2s_driver_uninstall(I2S_NUM_0);
        return false;
    }

    initialized_ = true;
    log_i("I2S microphone initialized (bclk=%d, ws=%d, din=%d)", bclk_pin_, ws_pin_, data_pin_);
    return true;
}

bool micro_rtsp_source_audio_i2s::update()
{
    if (!initialized_)
        return false;

    // Read one chunk of 16 kHz PCM samples (20 ms -> 320 samples -> 640 bytes)
    const size_t samples_to_read = (i2s_sample_rate * 20) / 1000;
    const size_t bytes_to_read = samples_to_read * sizeof(int16_t);

    size_t bytes_read = 0;
    auto err = i2s_read(I2S_NUM_0, pcm_buffer_, bytes_to_read, &bytes_read, pdMS_TO_TICKS(20));
    if (err != ESP_OK)
    {
        log_e("I2S read failed: 0x%x", err);
        return false;
    }

    const size_t samples_read = bytes_read / sizeof(int16_t);
    // Downsample 16 kHz -> 8 kHz (take every second sample) and encode a-law
    alaw_size_ = 0;
    for (size_t i = 0; i < samples_read && alaw_size_ < alaw_capacity_; i += 2)
        alaw_buffer_[alaw_size_++] = linear_to_alaw(pcm_buffer_[i]);

    return alaw_size_ > 0;
}

// G.711 a-law encoder (ITU-T G.711), 16 bit PCM in -> 8 bit a-law out.
uint8_t micro_rtsp_source_audio_i2s::linear_to_alaw(int16_t pcm)
{
    uint8_t sign = (pcm >> 8) & 0x80;
    if (sign != 0)
        pcm = (int16_t)-pcm;
        
    if (pcm > 32635)
        pcm = 32635;

    uint8_t exponent = 7;
    uint16_t exp_mask = 0x4000;
    while (exponent > 0 && (pcm & exp_mask) == 0)
    {
        exponent--;
        exp_mask >>= 1;
    }

    uint8_t mantissa = (pcm >> (exponent + 3)) & 0x0f;
    uint8_t alaw = sign | (uint8_t)(exponent << 4) | mantissa;

    // European (even bit) toggle, RFC 3551 PCMA
    return (uint8_t)(alaw ^ 0x55);
}
