// saver.h - сохранение кадров

#pragma once

#include <stdbool.h>
#include <stdint.h>

void saver_init(const char * prefix, const char * outdir, int batch_size, int time_window, int width, int height,
                int bytes_per_pixel, const char * format_name);
void saver_add_frame(const uint8_t * frame_data);
void saver_flush(void);
void saver_toggle_recording(void);
void saver_cleanup(void);

// Текущее состояние записи (true - идёт запись)
bool saver_is_recording(void);
