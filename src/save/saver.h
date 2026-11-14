#pragma once

#include "util/types.h"
#include <stdbool.h>
#include <stdint.h>

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
void saver_cleanup(void);

void saver_recording_frame_add(const uint8_t * frame_data);
void saver_recording_flush(void);
void saver_recording_toggle(void);
bool saver_recording_is_run(void);

void saver_outdir_open(void);
const char * saver_outdir_get(void);

const char * saver_prefix_get(void);
const char * saver_format_name_get(void);
