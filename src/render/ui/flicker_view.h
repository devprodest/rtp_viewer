#pragma once

#include "render/gl/gl_api.h"
#include "util/types.h"

#include <stdbool.h>

bool flicker_view_init(GLuint quad_vao);
void flicker_view_cleanup(void);
void flicker_view_update(void);
void flicker_view_render(point_s pos, image_size_s win, float dpi_scale);

/// \brief Рисует текст аналитики под графиком. Вызывать внутри overlay_begin/overlay_end.
void flicker_view_draw_text(point_s pos, float dpi_scale);

void flicker_view_toggle_enabled(void);
bool flicker_view_is_enabled(void);
void flicker_view_disable(void);

// --- Позиция и размер панели ---
point_s flicker_view_get_pos(void);
void flicker_view_set_pos(point_s pos);
image_size_s flicker_view_get_size(void);
