#pragma once

#include <stdbool.h>

#include "util/types.h"

void window_title_init(const char * format_name, image_size_s frame_size);
void window_title_set_paused(bool paused);
void window_title_set_sharpness(bool valid, double lap, double ten);
void window_title_refresh(void);
