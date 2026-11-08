#pragma once

#include "render/gl/gl_api.h"
#include "util/types.h"

#include <stdbool.h>

bool histogram_view_init(GLuint quad_vao);
void histogram_view_cleanup(void);

void histogram_view_update(int effective_bits);
void histogram_view_render(point_s pos, image_size_s win, float dpi_scale);

int histogram_view_get_active_channels(void);

void histogram_view_disable(void);
void histogram_view_toggle_enabled(void);
bool histogram_view_is_enabled(void);
void histogram_view_toggle_log_scale(void);
bool histogram_view_is_log_scale(void);

// --- Позиция и размер панели (для перетаскивания) ---
point_s histogram_view_get_pos(void);
void histogram_view_set_pos(point_s pos);
image_size_s histogram_view_get_size(void);
