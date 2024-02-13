#pragma once

#include <stdint.h>
#include <stddef.h>
#include <tuple>

class micro_rtsp_jpeg
{
public:
    micro_rtsp_jpeg(const uint8_t *jpeg, size_t size);

protected:
    bool decode_jpeg(uint8_t *jpg, size_t size);
    std::tuple<const uint8_t *, size_t> find_jpeg_section(uint8_t **ptr, uint8_t *end, uint8_t flag);

    std::tuple<const uint8_t *, size_t> quantization_table_0_;
    std::tuple<const uint8_t *, size_t> quantization_table_1_;
    std::tuple<const uint8_t *, size_t> jpeg_data_;
};