#include <stddef.h>

#include "jpg_section.h"

class jpg
{
public:
    bool decode(const uint8_t *jpg, size_t size);

    const jpg_section_dqt_t *quantization_table_luminance_;
    const jpg_section_dqt_t *quantization_table_chrominance_;

    const uint8_t *jpeg_data_start;
    const uint8_t *jpeg_data_end;

private:
    static const jpg_section_t *find_jpg_section(const uint8_t **ptr, const uint8_t *end, jpg_section_t::jpg_section_flag flag);
};