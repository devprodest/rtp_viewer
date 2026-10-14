#pragma once

#include <stdbool.h>

#include "gl_api.h"

bool histogram_view_init(GLuint quad_vao);
void histogram_view_cleanup(void);
void histogram_view_update(void);

void histogram_view_render(int x, int y, int w, int h);

void histogram_view_toggle_enabled(void);
void histogram_view_toggle_log_scale(void);
void histogram_view_toggle_cdf(void);

bool histogram_view_is_enabled(void);
int  histogram_view_get_active_channels(void);