#pragma once

#include <Arduino.h>

typedef struct
{
    const char *key;
    const String value;
} template_variable_t;

template <typename T, size_t n>
inline String template_render(const String& format, T (&values)[n])
{
    auto s = String(format);
    // Conditional sections
    for (size_t i = 0; i < n; i++)
    {
        // Include Section {{#expr}}
        auto match_section_begin = "{{#" + String(values[i].key) + "}}";
        // Inverted section {{^expr}}
        auto match_section_inverted_begin = "{{^" + String(values[i].key) + "}}";
        // End section {{/expr}}
        auto match_section_end = "{{/" + String(values[i].key) + "}}";
        while (true)
        {
            bool inverted = false;
            auto first = s.indexOf(match_section_begin);
            if (first < 0)
            {
                inverted = true;
                first = s.indexOf(match_section_inverted_begin);
                if (first < 0)
                    break;
            }

            auto second = s.indexOf(match_section_end, first + match_section_begin.length());
            if (second < 0)
                break;

            // Arduino returns 0 and 1 for bool.toString()
            if ((!inverted && (values[i].value == "1")) || (inverted && (values[i].value == "0")))
                s = s.substring(0, first) + s.substring(first + match_section_begin.length(), second) + s.substring(second + match_section_end.length());
            else
                s = s.substring(0, first) + s.substring(second + match_section_end.length());
        }
    }

    // Replace variables {{variable}}
    for (size_t i = 0; i < n; i++)
        s.replace("{{" + String(values[i].key) + "}}", values[i].value);

    return s;
}