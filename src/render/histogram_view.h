#pragma once

#include "gl_api.h"
#include "util/types.h"

#include <stdbool.h>

bool histogram_view_init(GLuint quad_vao);
void histogram_view_cleanup(void);
void histogram_view_update(void);

void histogram_view_render(point_s s, image_size_s size);

void histogram_view_toggle_enabled(void);
void histogram_view_toggle_log_scale(void);
void histogram_view_toggle_cdf(void);

bool histogram_view_is_enabled(void);
int  histogram_view_get_active_channels(void);

bool histogram_view_is_log_scale(void);
bool histogram_view_is_cdf(void);
