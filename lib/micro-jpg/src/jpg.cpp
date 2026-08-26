#include <esp32-hal-log.h>
#include "jpg.h"

const jpg_section_t *jpg::find_jpg_section(const uint8_t **ptr, const uint8_t *end, jpg_section_t::jpg_section_flag flag)
{
    log_d("find_jpeg_section 0x%02x (%s)", flag, jpg_section_t::flag_name(flag));
    while (*ptr < end)
    {
        // At least the framing and marker bytes must be present
        const size_t remaining = (size_t)(end - *ptr);
        if (remaining < 2)
            break;

        // flag, len MSB, len LSB
        auto section = reinterpret_cast<const jpg_section_t *>((*ptr));
        if (section->framing != 0xff)
        {
            log_e("Expected framing 0xff but found: 0x%02x", section->framing);
            break;
        }

        if (!jpg_section_t::is_valid_flag(section->flag))
        {
            log_d("Unknown section 0x%02x", section->flag);
            return nullptr;
        }

        // Length-less markers (SOI, EOI) are 2 bytes; the others carry a
        // 2 byte length field which must also fit inside the buffer
        const bool has_length = section->flag != jpg_section_t::jpg_section_flag::SOI &&
                                section->flag != jpg_section_t::jpg_section_flag::EOI;
        if (has_length && remaining < 4)
        {
            log_e("Truncated section 0x%02x", section->flag);
            break;
        }

        // Advance pointer. Note: a section has a length, so not SOI (0xd8) and EOI (0xd9)
        const size_t length = section->section_length();
        if (length > remaining)
        {
            log_e("Section 0x%02x length %u exceeds remaining %u bytes",
                  section->flag, (unsigned)length, (unsigned)remaining);
            break;
        }

        *ptr += length;
        if (section->flag == flag)
        {
            log_d("Found section 0x%02x (%s), %d bytes", flag, jpg_section_t::flag_name(section->flag), (int)length);
            return section;
        }

        log_d("Skipping section: 0x%02x (%s), %d bytes", section->flag, jpg_section_t::flag_name(section->flag), (int)length);
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

    // Quantization tables. ESP32-CAM emits one segment per table, luminance
    // (id=0) followed by chrominance (id=1), but the tables are assigned by
    // their id so the result is correct even if they appear in a different
    // order.
    quantization_table_luminance_ = nullptr;
    quantization_table_chrominance_ = nullptr;
    while (quantization_table_luminance_ == nullptr || quantization_table_chrominance_ == nullptr)
    {
        const jpg_section_t *quantization_table_section = find_jpg_section(&ptr, end, jpg_section_t::jpg_section_flag::DQT);
        if (quantization_table_section == nullptr)
        {
            log_e("No (more) quantization table sections found");
            return false;
        }

        if (quantization_table_section->data_length() != sizeof(jpg_section_dqt_t))
        {
            log_w("Invalid quantization table section length. Expected %d but read %d",
                  sizeof(jpg_section_dqt_t), quantization_table_section->data_length());
            return false;
        }

        auto table = reinterpret_cast<const jpg_section_dqt_t *>(quantization_table_section->data);
        if (table->id == 0)
            quantization_table_luminance_ = table;
        else if (table->id == 1)
            quantization_table_chrominance_ = table;
    }

    // Start of scan
    if (!find_jpg_section(&ptr, end, jpg_section_t::jpg_section_flag::SOS))
    {
        log_e("No start of scan section found");
        return false;
    }

    // Start of the data sections
    jpeg_data_start = ptr;

    log_d("Skipping over data sections");
    // Skip the entropy-coded bytes up to the next marker (0xff followed by a
    // non-zero byte). Stuffed bytes (0xff 0x00) are part of the data. When a
    // restart marker (RST0..RST7, which have no length field) is encountered
    // there is more entropy-coded data after it, so keep scanning.
    while (ptr < end - 1)
    {
        while (ptr < end - 1 && (ptr[0] != 0xff || ptr[1] == 0))
            ptr++;
        if (ptr >= end - 1)
            break;

        const uint8_t marker = ptr[1];
        if (marker >= jpg_section_t::jpg_section_flag::RST0 &&
            marker <= jpg_section_t::jpg_section_flag::RST7)
        {
            ptr += 2;
            continue;
        }
        break;
    }

    // Check if marker is an end of image marker
    if (!find_jpg_section(&ptr, end, jpg_section_t::jpg_section_flag::EOI))
    {
        log_e("No end of image marker found");
        return false;
    }

    jpeg_data_end = ptr;

    log_d("Total jpeg data: %d bytes", jpeg_data_end - jpeg_data_start);

    return true;
}
