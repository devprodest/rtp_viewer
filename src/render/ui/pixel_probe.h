#pragma once

#include "util/types.h"
#include <stdint.h>


typedef struct pixel_sample_s
{
    color_rgb_s rgb;
    uint32_t raw;
} pixel_sample_s;


void pixel_probe_toggle(void);
bool pixel_probe_is_enabled(void);

void pixel_probe_set_frame(const uint8_t * data, image_size_s frame_size, int bbp);

void pixel_probe_set_mouse(point_s screen);
void pixel_probe_draw(image_size_s win, float dpi_scale);

point_s pixel_probe_get_mouse(void);
image_size_s pixel_probe_get_frame_size(void);
int pixel_probe_get_bpp(void);
pixel_sample_s pixel_probe_read_pixel(point_s pixel);
