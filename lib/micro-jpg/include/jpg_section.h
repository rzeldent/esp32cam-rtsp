#pragma once

#include <stddef.h>
#include <stdint.h>

//  http://www.ietf.org/rfc/rfc2345.txt Each table is an array of 64 values given in zig-zag order, identical to the format used in a JFIF DQT marker segment.
constexpr size_t jpeg_quantization_table_length = 64;

typedef struct __attribute__((packed))
{
    enum jpg_section_flag : uint8_t
    {
        DATA = 0x00,
        SOF0 = 0xc0,
        SOF1 = 0xc1,
        SOF2 = 0xc2,
        SOF3 = 0xc3,
        DHT = 0xc4,
        SOF5 = 0xc5,
        SOF6 = 0xc6,
        SOF7 = 0xc7,
        JPG = 0xc8,
        SOF9 = 0xc9,
        SOF10 = 0xca,
        SOF11 = 0xcb,
        DAC = 0xcc,
        SOF13 = 0xcd,
        SOF14 = 0xce,
        SOF15 = 0xcf,
        RST0 = 0xd0,
        RST1 = 0xd1,
        RST2 = 0xd2,
        RST3 = 0xd3,
        RST4 = 0xd4,
        RST5 = 0xd5,
        RST6 = 0xd6,
        RST7 = 0xd7,
        SOI = 0xd8,
        EOI = 0xd9,
        SOS = 0xda,
        DQT = 0xdb,
        DNL = 0xdc,
        DRI = 0xdd,
        DHP = 0xde,
        EXP = 0xdf,
        APP0 = 0xe0,
        APP1 = 0xe1,
        APP2 = 0xe2,
        APP3 = 0xe3,
        APP4 = 0xe4,
        APP5 = 0xe5,
        APP6 = 0xe6,
        APP7 = 0xe7,
        APP8 = 0xe8,
        APP9 = 0xe9,
        APP10 = 0xea,
        APP11 = 0xeb,
        APP12 = 0xec,
        APP13 = 0xed,
        APP14 = 0xee,
        APP15 = 0xef,
        JPG0 = 0xf0,
        JPG1 = 0xf1,
        JPG2 = 0xf2,
        JPG3 = 0xf3,
        JPG4 = 0xf4,
        JPG5 = 0xf5,
        JPG6 = 0xf6,
        JPG7 = 0xf7,
        JPG8 = 0xf8,
        JPG9 = 0xf9,
        COM = 0xfe,
        JPG10 = 0xfa,
        JPG11 = 0xfb,
        JPG12 = 0xfc,
        JPG13 = 0xfd
    };

    const uint8_t framing; // 0xff
    const jpg_section_flag flag;
    const uint8_t length_msb;
    const uint8_t length_lsb;
    const uint8_t data[];

    static bool is_valid_flag(const jpg_section_flag flag);
    static const char *flag_name(const jpg_section_flag flag);
    uint16_t data_length() const;
    uint16_t section_length() const;
} jpg_section_t;

typedef struct __attribute__((packed)) // 0xffe0
{
    char identifier[5] = {'J', 'F', 'I', 'F', 0}; // JFIF identifier, zero-terminated
    uint8_t version_major = 1;
    uint8_t version_minor = 1; // JFIF version 1.1
    uint8_t density_units = 0; // no density units specified
    uint16_t density_hor = 1;
    uint16_t density_ver = 1; // density: 1 pixel "per pixel" horizontally and vertically
    uint8_t thumbnail_hor = 0;
    uint8_t thumbnail_ver = 0; // no thumbnail (size 0 x 0)
} jpg_section_app0_t;

typedef struct __attribute__((packed)) // 0xffdb
{
    uint8_t id; // 0= quantLuminance, 1= quantChrominance
    uint8_t data[jpeg_quantization_table_length];
} jpg_section_dqt_t;