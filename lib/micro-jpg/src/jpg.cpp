#include <esp32-hal-log.h>
#include "jpg.h"

const jpg_section_t *jpg::find_jpg_section(const uint8_t **ptr, const uint8_t *end, jpg_section_t::jpg_section_flag flag)
{
    log_d("find_jpeg_section 0x%02x (%s)", flag, jpg_section_t::flag_name(flag));
    while (*ptr < end)
    {
        // flag, len MSB, len LSB
        auto section = reinterpret_cast<const jpg_section_t *>((*ptr));
        if (section->framing != 0xff)
        {
            log_e("Expected framing 0xff but found: 0x%02x", section->framing);
            break;
        }

        if (!jpg_section_t::is_valid_flag(section->flag))
        {
            log_d("Unknown section 0x%02x", flag);
            return nullptr;
        }

        // Advance pointer section has a length, so not SOI (0xd8) and EOI (0xd9)
        *ptr += section->section_length();
        if (section->flag == flag)
        {
            log_d("Found section 0x%02x (%s), %d bytes", flag, jpg_section_t::flag_name(section->flag), section->section_length());
            return section;
        }

        log_d("Skipping section: 0x%02x (%s), %d bytes", section->flag, jpg_section_t::flag_name(section->flag), section->section_length());
    }

    // Not found
    return nullptr;
}

// See https://create.stephan-brumme.com/toojpeg/
bool jpg::decode(const uint8_t *data, size_t size)
{
    log_d("decode_jpeg");
    // Look for start jpeg file (0xd8)
    auto ptr = data;
    auto end = ptr + size;

    // Check for SOI (start of image) 0xff, 0xd8
    if (!find_jpg_section(&ptr, end, jpg_section_t::jpg_section_flag::SOI))
    {
        log_e("No valid start of image marker found");
        return false;
    }

    // First quantization table (Luminance - black & white images)
    if (!(quantization_table_luminance_ = find_jpg_section(&ptr, end, jpg_section_t::jpg_section_flag::DQT)))
    {
        log_e("No quantization table 0 section found");
        return false;
    }

    // Second quantization table (Chrominance - color images)
    if (!(quantization_table_chrominance_ = find_jpg_section(&ptr, end, jpg_section_t::jpg_section_flag::DQT)))
        log_w("No quantization table 1 section found");

    // Start of scan
    if (!find_jpg_section(&ptr, end, jpg_section_t::jpg_section_flag::SOS))
    {
        log_e("No start of scan section found");
        return false;
    }

    // Start of the data sections
    jpeg_data_start = ptr;

    log_d("Skipping over data sections");
    // Scan over all the sections. 0xff followed by not zero, is a new section
    while (ptr < end - 1 && (ptr[0] != 0xff || ptr[1] == 0))
        ptr++;

    // Check if marker is an end of image marker
    if (!find_jpg_section(&ptr, end, jpg_section_t::jpg_section_flag::EOI))
    {
        log_e("No end of image marker found");
        return false;
    }

    log_d("Total jpeg data = %d bytes", jpeg_data_end - jpeg_data_start);

    return true;
}
