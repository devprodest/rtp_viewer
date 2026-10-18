#pragma once

#include <stdbool.h>

typedef struct format_config_s
{
    int bytes_per_pixel;
    int gl_internal_format;
    int gl_format;
    int gl_type;
    char shader_file[ 256 ];
    char bayer_pattern[ 8 ];
    bool needs_demosaic;
    bool is_ycbcr;
} format_config_s;

bool format_config_load(const char * format_name, format_config_s * cfg);
