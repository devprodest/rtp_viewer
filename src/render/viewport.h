#pragma once

#include <stdbool.h>

#include "util/types.h"

void viewport_init(image_size_s frame_size);
void viewport_update(image_size_s win);

point_s viewport_get_point(void);
image_size_s viewport_get_size(void);

void viewport_reset(void);
void viewport_apply_wheel(double wheel, point_s screen);
void viewport_apply_pan_drag(point_s screen, bool middle_down);

bool viewport_screen_to_frame(point_s screen, point_s * frame);
bool viewport_frame_to_screen(point_s frame, point_s * screen);
