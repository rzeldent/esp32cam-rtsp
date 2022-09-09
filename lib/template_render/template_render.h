#pragma once

#include <Wstring.h>

typedef struct
{
    const char *key;
    const String value;
} template_substitution_t;

template<typename T, size_t n>
inline String template_render(const char *format, T (&values)[n])
{
    auto s = String(format);
     for (size_t i=0; i<n; i++)
        s.replace("{{" + String(values[n].key) + "}}", values[n].value);

    return s;
}