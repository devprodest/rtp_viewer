#pragma once

#include "analysis/selection.h"
#include "format/format_config.h"
#include "render/gl/gl_api.h"
#include "util/types.h"

#include <stdbool.h>

bool vectorscope_view_init(GLuint quad_vao);
void vectorscope_view_cleanup(void);
void vectorscope_view_update(void);

void vectorscope_view_render(point_s pos, image_size_s win, float dpi_scale);

void vectorscope_view_toggle_enabled(void);
bool vectorscope_view_is_enabled(void);
void vectorscope_view_disable(void);

// --- Позиция и размер панели ---
point_s vectorscope_view_get_pos(void);
void vectorscope_view_set_pos(point_s pos);
image_size_s vectorscope_view_get_size(void);
