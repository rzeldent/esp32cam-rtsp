#include <cstring>

#include <esp32-hal-log.h>

#include <jpg.h>

#include "micro_rtsp_jpeg_header.h"

micro_rtsp_jpeg_header::micro_rtsp_jpeg_header()
    : valid_(false),
      scan_start_offset_(0),
      scan_start_(nullptr),
      scan_end_(nullptr)
{
}

bool micro_rtsp_jpeg_header::prepare(const uint8_t *data, size_t size)
{
    // The header (quantization tables and scan data start) is constant for a
    // fixed quality / frame size, so reuse it when the cached copy matches.
    // This skips the full parse which would otherwise walk every entropy-coded
    // byte to locate EOI.
    if (valid_ &&
        size > scan_start_offset_ + 2 &&
        data[0] == 0xff && data[1] == 0xd8 &&                 // SOI
        data[size - 2] == 0xff && data[size - 1] == 0xd9)     // EOI at the end
    {
        scan_start_ = data + scan_start_offset_;
        scan_end_ = data + size;
        return true;
    }

    jpg jpg;
    if (!jpg.decode(data, size))
    {
        log_e("Unable to decode JPEG frame");
        return false;
    }
    if (jpg.quantization_table_luminance_ == nullptr || jpg.quantization_table_chrominance_ == nullptr)
    {
        log_e("JPEG frame is missing quantization tables");
        return false;
    }

    memcpy(quant_lum_, jpg.quantization_table_luminance_->data, jpeg_qtable_size);
    memcpy(quant_chr_, jpg.quantization_table_chrominance_->data, jpeg_qtable_size);
    scan_start_offset_ = (size_t)(jpg.jpeg_data_start - data);
    valid_ = true;

    scan_start_ = jpg.jpeg_data_start;
    scan_end_ = jpg.jpeg_data_end;

    return true;
}
