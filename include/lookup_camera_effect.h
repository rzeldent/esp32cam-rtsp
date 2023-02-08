#pragma once

#include <string.h>

typedef char camera_effect_name_t[11];
typedef struct
{
    const camera_effect_name_t name;
    const int value;
} camera_effect_entry_t;

constexpr const camera_effect_entry_t camera_effects[] = {
    {"Normal", 0},
    {"Negative", 1},
    {"Grayscale", 2},
    {"Red tint", 3},
    {"Green tint", 4},
    {"Blue tint", 5},
    {"Sepia", 6}};

const int lookup_camera_effect(const char *name)
{
    // Lookup table for the frame name to framesize_t
    for (const auto &entry : camera_effects)
        if (strncmp(entry.name, name, sizeof(camera_effect_entry_t)) == 0)
            return entry.value;

    return 0;
}
