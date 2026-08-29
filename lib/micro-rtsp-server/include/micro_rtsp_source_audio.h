#pragma once

#include <stddef.h>
#include <stdint.h>

// Interface for an optional audio source (e.g. an I2S microphone).
// The source is responsible for capturing and encoding the samples.
// The RTSP server reads complete chunks of G.711 a-law encoded samples (one byte per 8 kHz sample) and packetizes them into RTP packets.
class micro_rtsp_source_audio
{
public:
    virtual ~micro_rtsp_source_audio() = default;

    // Capture the next chunk of audio. Returns true when new samples are available (data()/size() are valid).
    virtual bool update() = 0;

    // G.711 a-law encoded samples (one byte per sample).
    virtual const uint8_t *data() const = 0;
    virtual size_t size() const = 0;
    // 8000 for G.711 a-law telephony audio.
    virtual uint32_t sample_rate() const = 0;
    virtual uint8_t channels() const = 0;
};
