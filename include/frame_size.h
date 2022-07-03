#pragma once

#include <string.h>
#include <sensor.h>

typedef char frame_size_name_t[18];

typedef struct frame_size_entry
{
    frame_size_name_t name;
    framesize_t frame_size;
} frame_size_entry_t;

constexpr const frame_size_entry_t frame_sizes[] = {

    {"96x96", FRAMESIZE_96X96},
    {"QQVGA (160x120)", FRAMESIZE_QQVGA},
    {"QCIF (176x144)", FRAMESIZE_QCIF},
    {"HQVGA (240x176)", FRAMESIZE_HQVGA},
    {"240x240", FRAMESIZE_240X240},
    {"QVGA (320x240)", FRAMESIZE_QVGA},
    {"CIF (400x296)", FRAMESIZE_CIF},
    {"HVGA (480x320)", FRAMESIZE_HVGA},
    {"VGA (640x480)", FRAMESIZE_VGA},
    {"SVGA (800x600)", FRAMESIZE_SVGA},
    {"XGA (1024x768)", FRAMESIZE_XGA},
    {"HD (1280x720)", FRAMESIZE_HD},
    {"SXGA (1280x1024)", FRAMESIZE_SXGA},
    {"UXGA (1600x1200)", FRAMESIZE_UXGA},
    {"FHD (1920x1080)", FRAMESIZE_FHD},
    {"P HD (2560x1440)", FRAMESIZE_P_HD},
    {"P 3MP (2560x1600)", FRAMESIZE_P_3MP},
    {"QXGA (2560x1920)", FRAMESIZE_QXGA},
    {"QHD (2560x1440)", FRAMESIZE_QHD},
    {"WQXGA (2560x1600)", FRAMESIZE_WQXGA},
    {"P FHD (1080x1920)", FRAMESIZE_P_FHD},
    {"QSXGA (2560x1920)", FRAMESIZE_QSXGA},
    {"", FRAMESIZE_INVALID}};

framesize_t lookup_frame_size(const char *pin)
{
    // Lookup table for the frame name to framesize_t
    auto entry = &frame_sizes[0];
    while (*entry->name)
    {
        if (strncmp(entry->name, pin, sizeof(frame_size_name_t)) == 0)
            return entry->frame_size;

        entry++;
    }

    return FRAMESIZE_INVALID;
}