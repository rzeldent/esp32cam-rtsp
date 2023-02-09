#pragma once

#include <string.h>
#include <esp_camera.h>

typedef struct
{
    const char name[5];
    const gainceiling_t value;
} camera_gainceiling_entry_t;

constexpr const camera_gainceiling_entry_t camera_gain_ceilings[] = {
    {"2X", GAINCEILING_2X},
    {"4X", GAINCEILING_4X},
    {"8X", GAINCEILING_8X},
    {"16X", GAINCEILING_16X},
    {"32X", GAINCEILING_32X},
    {"64X", GAINCEILING_64X},
    {"128X", GAINCEILING_128X}};

const gainceiling_t lookup_camera_gainceiling(const char *name)
{
    // Lookup table for the frame name to framesize_t
    for (const auto &entry : camera_gain_ceilings)
        if (strncmp(entry.name, name, sizeof(entry.name)) == 0)
            return entry.value;

    return GAINCEILING_2X;
}
