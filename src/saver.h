// saver.h - сохранение кадров

#ifndef SAVER_H
#define SAVER_H

#include <stdint.h>
#include <stdbool.h>

void saver_init(const char *prefix, const char *outdir,
                int batch_size, int time_window,
                int width, int height, int bytes_per_pixel);
void saver_add_frame(const uint8_t *frame_data);
void saver_flush(void);
void saver_toggle_recording(void);
void saver_cleanup(void);

#endif
