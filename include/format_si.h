#pragma once

#include <Arduino.h>

String format_value(float value)
{
    if (value == 0.0)
        return "0";

    if (value < 0)
        return "-" + format_value(-value);

    // No decimal places
    if (value >= 1)
        return String(value, 0);
    if (value < 0.001f)
        return String(value, 4);
    if (value < 0.01f)
        return String(value, 3);
    if (value < 0.1f)
        return String(value, 2);
    return String(value, 1);
}

String format_si(double value, int decimal_places = 2)
{
    if (value == 0.0)
        return "0";

    if (value < 0)
        return "-" + format_si(-value, decimal_places);

    auto value_abs = fabs(value);
    if (value_abs < 1E-9)
        return String(value * 1E9, decimal_places) + "p";
    if (value_abs < 1E-6)
        return String(value * 1E9, decimal_places) + "n";
    if (value_abs < 1E-3)
        return String(value * 1E6, decimal_places) + "u";
    if (value_abs < 1)
        return String(value * 1E3, decimal_places) + "m";
    if (value_abs < 1E3)
        return String(value, decimal_places);
    if (value_abs < 1E6)
        return String(value / 1E3, decimal_places) + "k";
    if (value_abs < 1E9)
        return String(value / 1E6, decimal_places) + "M";
    if (value_abs < 1E12)
        return String(value / 1E9, decimal_places) + "G";
    if (value_abs < 1E15)
        return String(value / 1E12, decimal_places) + "T";
        
    return "NaN";
}