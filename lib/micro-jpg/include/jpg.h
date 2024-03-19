#include <stddef.h>

#include "jpg_section.h"

class jpg
{
public:
    bool decode(const uint8_t *jpg, size_t size);

    const struct jpg_section *quantization_table_0_;
    const struct jpg_section *quantization_table_1_;

    const uint8_t *jpeg_data_start;
    const uint8_t *jpeg_data_end;

private:
    static const jpg_section *find_jpg_section(const uint8_t **ptr, const uint8_t *end, jpg_section::jpg_section_flag flag);
};