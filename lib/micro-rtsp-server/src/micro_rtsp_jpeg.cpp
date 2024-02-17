#include <esp32-hal-log.h>

#include "micro_rtsp_jpeg.h"

const char *micro_rtsp_jpeg::jpg_section::flag_name() const
{
    switch (section_flag)
    {
    case 0xd8:
        return "SOI"; // start of image
    case 0xd9:
        return "EOI"; // end of image
    case 0xe0:
        return "APP0";
    case 0xdb:
        return "DQT"; // define quantization table
    case 0xc4:
        return "DHT"; // define Huffman table
    case 0xc0:
        return "SOF"; // start of frame
    case 0xda:
        return "SOS"; // start of scan
    }
    return "Unknown";
}

const micro_rtsp_jpeg::jpg_section *micro_rtsp_jpeg::find_jpeg_section(const uint8_t **ptr, const uint8_t *end, uint8_t flag)
{
    log_d("find_jpeg_section 0x%02x", flag);
    while (*ptr < end)
    {
        auto framing = *((*ptr)++);
        if (framing != 0xff)
        {
            log_e("Expected framing 0xff but found: 0x%02x", framing);
            break;
        }

        // framing = 0xff, flag, len MSB, len LSB
        auto section = reinterpret_cast<const jpg_section *>((*ptr)++);
        // Advance pointer section has a length, so not SOI (0xd8) and EOI (0xd9)
        *ptr += section->section_length();
        if (section->flag() == flag)
        {
            log_d("Found section 0x%02x (%s), %d bytes", flag, section->flag_name(), section->section_length());
            return section;
        }

        log_d("Skipping section: 0x%02x (%s), %d bytes", section->flag(), section->flag_name(), section->section_length());
    }

    // Not found
    return nullptr;
}

// See https://create.stephan-brumme.com/toojpeg/
bool micro_rtsp_jpeg::decode_jpeg(const uint8_t *data, size_t size)
{
    log_d("decode_jpeg");
    // Look for start jpeg file (0xd8)
    auto ptr = data;
    auto end = ptr + size;

    // Check for SOI (start of image) 0xff, 0xd8
    if (!find_jpeg_section(&ptr, end, 0xd8))
    {
        log_e("No valid start of image marker found");
        return false;
    }

    // First quantization table
    if (!(quantization_table_0_ = find_jpeg_section(&ptr, end, 0xdb)))
        log_e("No quantization table 0 section found");

    // Second quantization table (for color images)
    if (!(quantization_table_1_ = find_jpeg_section(&ptr, end, 0xdb)))
        log_w("No quantization table 1 section found");

    // Start of scan
    if (!find_jpeg_section(&ptr, end, 0xda))
        log_e("No start of scan section found");

    // Start of the data sections
    jpeg_data_start = ptr;

    // Scan over all the sections. 0xff followed by not zero, is a new section
    while (ptr < end - 1 && (ptr[0] != 0xff || ptr[1] == 0))
        ptr++;

    // Check if marker is an end of image marker
    if (!find_jpeg_section(&ptr, end, 0xd9))
    {
        log_e("No end of image marker found");
        return false;
    }

    jpeg_data_end = ptr;
    log_d("Total jpeg data = %d bytes", jpeg_data_end - jpeg_data_start);

    return true;
}
