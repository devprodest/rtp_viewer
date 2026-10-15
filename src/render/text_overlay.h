#pragma once


void text_overlay_init(void);
void text_overlay_cleanup(void);

void text_begin(int win_w, int win_h);
void text_draw(int x, int y, const char * text, float r, float g, float b, float scale);

int text_measure(const char * text, float scale);

void text_end(void);

void overlay_rect(int x0, int y0, int x1, int y1, float r, float g, float b, float a);
void overlay_rect_outline(int x0, int y0, int x1, int y1, float r, float g, float b, float a, int thickness);

