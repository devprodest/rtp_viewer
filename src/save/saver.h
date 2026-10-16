#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "util/types.h"

typedef struct saver_config_s
{
    const char * prefix;
    const char * outdir;
    int batch_size;
    int time_window;
    image_size_s image;
    int bpp;
    const char * format_name;
} saver_config_s;

void saver_init(saver_config_s * config);
void saver_add_frame(const uint8_t * frame_data);
void saver_flush(void);
void saver_toggle_recording(void);
void saver_cleanup(void);

bool saver_is_recording(void);
const char * saver_get_outdir(void);
