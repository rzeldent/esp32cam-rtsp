#pragma once

#include <stddef.h>
#include <stdint.h>

#include "jpg_section.h"

// Caches the parts of a JPEG frame that do not change between frames for a
// fixed quality / frame size: the luminance and chrominance quantization
// tables plus the offset of the start-of-scan marker. Once populated, the
// per-frame entropy-coded scan data is never parsed again.
class jpeg_header
{
public:
    jpeg_header();

    // Prepare the given frame buffer for streaming. Reuses the cached header
    // when it still matches, otherwise decodes the frame and refreshes the
    // cache. On success sets scan_start()/scan_end() to the scan data range
    // (start-of-scan up to and including EOI) and returns true. Returns false
    // when the frame cannot be decoded.
    bool prepare(const uint8_t *data, size_t size);

    const uint8_t *scan_start() const { return scan_start_; }
    const uint8_t *scan_end() const { return scan_end_; }
    const uint8_t *luminance() const { return quant_lum_; }
    const uint8_t *chrominance() const { return quant_chr_; }

private:
    // Frame-independent header info, constant for a fixed quality/frame size.
    bool valid_;
    size_t scan_start_offset_;
    uint8_t quant_lum_[jpeg_quantization_table_length];
    uint8_t quant_chr_[jpeg_quantization_table_length];

    // Scan data range of the most recently prepared frame.
    const uint8_t *scan_start_;
    const uint8_t *scan_end_;
};
