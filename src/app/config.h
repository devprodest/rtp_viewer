#pragma once

#include "util/types.h"
#include <stdbool.h>

typedef struct app_config_s
{
    // Видео
    image_size_s image;
    const char * format;

    // Источник данных
    int port;                 // -p, 0 если не задан
    const char * source_file; // -i, nullptr если не задан
    int mock_fps;

    // Save
    const char * save_prefix; // -s
    const char * outdir;      // -o
    int batch_size;           // -b
    int time_window;          // -t

    // Прочее
    const char * logfile; // -l
    int effective_bits;
    bool smooth_start;

    bool use_file_source; // вычисляется по source_file
} app_config_s;


bool app_config_parse(int argc, char ** argv, app_config_s * cfg, bool * exit_ok);

void app_config_print_usage(const char * prog);
