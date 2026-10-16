#pragma once

#include "util/types.h"

void overlay_init(void);
void overlay_cleanup(void);

void everlay_begin(image_size_s win);
void everlay_end(void);

void text_draw(point_s s, const char * text, fcolor_rgb_s color, float scale);
int text_measure(const char * text, float scale);

void overlay_rect(rect_s rect, fcolor_rgba_s color);
void overlay_rect_outline(rect_s rect, fcolor_rgba_s color, int thickness);

