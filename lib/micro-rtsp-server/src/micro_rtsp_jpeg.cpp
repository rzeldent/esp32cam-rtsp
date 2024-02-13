#include <esp32-hal-log.h>

#include "micro_rtsp_jpeg.h"

micro_rtsp_jpeg::micro_rtsp_jpeg(const uint8_t *jpeg, size_t size)
{
}

std::tuple<const uint8_t *, size_t> micro_rtsp_jpeg::find_jpeg_section(uint8_t **ptr, uint8_t *end, uint8_t flag)
{
    size_t len;
    while (*ptr < end)
    {
        auto framing = *(*ptr++);
        if (framing != 0xff)
        {
            log_e("Malformed jpeg. Expected framing 0xff but found: 0x%02x", framing);
            break;
        }

        // framing = 0xff, flag, len MSB, len LSB
        auto flag_code = *(*ptr++);
        // SOI and EOI have no length
        len = flag_code == 0xd8 || flag_code == 0xd9 ? 0 : *(*ptr++) * 256 + *(*ptr++);
        if (flag_code == flag)
            return std::tuple<const uint8_t *, size_t>(*ptr, len);

        // Skip the section
        switch (flag_code)
        {
        case 0xd8: // SOI (start of image)
        case 0xd9: // EOI (end of image)
        case 0xe0: // APP00
        case 0xdb: // DQT (define quantization table)
        case 0xc4: // DHT (define Huffman table)
        case 0xc0: // SOF0 (start of frame)
        case 0xda: // SOS (start of scan)
        {
            // length of section
            log_d("Skipping jpeg section flag: 0x%02x, %d bytes", flag_code, len);
            ptr += len;
            break;
        }
        default:
            log_e("Unexpected jpeg flag: 0x%02x", flag_code);
        }
    }

    // Not found
    return std::tuple<const uint8_t *, size_t>(nullptr, 0);
}

// See https://create.stephan-brumme.com/toojpeg/
bool micro_rtsp_jpeg::decode_jpeg(uint8_t *data, size_t size)
{
    // Look for start jpeg file (0xd8)
    auto ptr = data;
    auto end = ptr + size;

    // Check for SOI (start of image) 0xff, 0xd8
    auto soi = find_jpeg_section(&ptr, end, 0xd8);
    if (std::get<0>(soi) == nullptr)
    {
        log_e("No valid start of image marker found");
        return false;
    }

    // First quantization table
    quantization_table_0_ = find_jpeg_section(&ptr, end, 0xdb);
    if (std::get<0>(quantization_table_0_) == nullptr)
    {
        log_e("No quantization table 0 section found");
    }

    // Second quantization table (for color images)
    quantization_table_1_ = find_jpeg_section(&ptr, end, 0xdb);
    if (std::get<0>(quantization_table_1_) == nullptr)
    {
        log_e("No quantization table 1 section found");
    }
    // Start of scan
    auto sos = find_jpeg_section(&ptr, end, 0xda);
    if (std::get<0>(sos) == nullptr)
    {
        log_e("No start of scan section found");
    }


// the scan data uses byte stuffing to guarantee anything that starts with 0xff
// followed by something not zero, is a new section.  Look for that marker and return the ptr
// pointing there

    // Skip the scan
    while (ptr < end - 1 && (*ptr != 0xff || ptr[1] == 0))
        ptr++;

    ptr -= 2; // Go back to the 0xff (marker)

    auto eoi = find_jpeg_section(&ptr, end, 0xd9);
    if (std::get<0>(eoi) == nullptr)
    {
        log_e("No end of image marker found");
    }

    jpeg_data_ = std::tuple<const uint8_t *, size_t>(ptr, size - (ptr - data));
    return true;
}
