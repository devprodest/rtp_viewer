#pragma once

#include "analysis/selection.h"
#include "format/format_config.h"
#include "render/gl_api.h"
#include "util/types.h"

#include <stdbool.h>

bool vectorscope_view_init(GLuint quad_vao);
void vectorscope_view_cleanup(void);
void vectorscope_view_update(const uint8_t * data, image_size_s size, const format_config_s * fmt,
                             const selection_s * sel, int bytes_per_pixel);

void vectorscope_view_render(image_size_s win, float dpi_scale);

void vectorscope_view_toggle_enabled(void);
bool vectorscope_view_is_enabled(void);

