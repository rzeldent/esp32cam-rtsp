#pragma once

#include <string.h>

typedef struct
{
    const char name[7];
    const int value;
} camera_wb_mode_entry_t;

constexpr const camera_wb_mode_entry_t camera_wb_modes[] = {
    {"Auto", 0},
    {"Sunny", 1},
    {"Cloudy", 2},
    {"Office", 3},
    {"Home", 4}};

const int lookup_camera_wb_mode(const char *name)
{
    // Lookup table for the frame name to framesize_t
    for (const auto &entry : camera_wb_modes)
        if (strncmp(entry.name, name, sizeof(entry.name)) == 0)
            return entry.value;

    return 0;
}