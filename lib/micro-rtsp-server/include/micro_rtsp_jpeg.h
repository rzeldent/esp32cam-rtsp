#pragma once

#include <stdint.h>
#include <stddef.h>

class micro_rtsp_jpeg
{
public:
    bool decode_jpeg(const uint8_t *jpg, size_t size);

    class __attribute__ ((packed)) jpg_section
    {
    public:
        uint8_t flag() const { return section_flag; }
        const char *flag_name() const;
        uint16_t section_length() const { return section_flag == 0xd8 || section_flag == 0xd9 ? 0 : (section_length_msb << 8) + section_length_lsb; }
        const uint8_t *data() const { return reinterpret_cast<const uint8_t *>(&section_data[1]); }
        uint16_t data_length() const { return section_length() - 3; }

    private:
        const uint8_t section_flag;
        const uint8_t section_length_msb;
        const uint8_t section_length_lsb;
        const uint8_t section_data[];
    };

    const jpg_section *quantization_table_0_;
    const jpg_section *quantization_table_1_;

    const uint8_t *jpeg_data_start;
    const uint8_t *jpeg_data_end;

private:
    static const jpg_section *find_jpeg_section(const uint8_t **ptr, const uint8_t *end, uint8_t flag);
};