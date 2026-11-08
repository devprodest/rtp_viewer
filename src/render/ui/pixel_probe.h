#pragma once

#include <stdbool.h>

#include "util/types.h"

// --- Управление ---
void pixel_probe_toggle(void);
bool pixel_probe_is_enabled(void);
void pixel_probe_disable(void);

void pixel_probe_set_mouse(point_s screen);
point_s pixel_probe_get_mouse(void);

// --- Отрисовка ---
void pixel_probe_draw(image_size_s win, float dpi_scale);
