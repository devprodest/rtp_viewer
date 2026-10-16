#pragma once

#include <stdbool.h>
#include "util/types.h"

typedef struct app_config_s
{
    // Видео
    image_size_s image;
    const char *format;

    // Источник данных
    int         port;         // -p, 0 если не задан
    const char *source_file;  // -i, nullptr если не задан
    int         mock_fps;

    // Save
    const char *save_prefix;  // -s
    const char *outdir;       // -o
    int         batch_size;   // -b
    int         time_window;  // -t

    // Прочее
    const char *logfile;      // -l
    int         effective_bits;
    bool        smooth_start;

    bool use_file_source;     // вычисляется по source_file
} app_config_s;

/// \brief Разбирает argv в конфиг. При ошибке печатает usage/ошибку и возвращает false.
///
/// \details Не выходит из программы самостоятельно — только возвращает false.
///          Ранние выходы (-h, -V) обрабатываются внутри и тоже возвращают false,
///          но с флагом exit_ok=true.
bool app_config_parse(int argc, char ** argv, app_config_s * cfg, bool * exit_ok);

void app_config_print_usage(const char * prog);