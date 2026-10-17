#pragma once

#include "util/types.h"

void overlay_init(void);
void overlay_cleanup(void);

void overlay_begin(image_size_s win);
void overlay_end(void);

void overlay_text_draw(point_s s, const char * text, fcolor_rgb_s color, float scale);
int overlay_text_measure(const char * text, float scale);

void overlay_rect_draw(rect_s rect, fcolor_rgba_s color);
void overlay_rect_outline_draw(rect_s rect, fcolor_rgba_s color, int thickness);
