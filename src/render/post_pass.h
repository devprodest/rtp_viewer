#pragma once

#include <stdbool.h>

#include "gl_api.h"
#include "util/types.h"

bool post_pass_init(GLuint quad_vao);
void post_pass_cleanup(void);

void post_pass_set_format(int bytes_per_pixel, int effective_bits);
void post_pass_set_texel_size(fimage_size_s size);

// --- Состояние ---
void post_pass_toggle_saturation(void);
bool post_pass_is_saturation_active(void);
void post_pass_toggle_gradient(void);
bool post_pass_is_gradient_active(void);
void post_pass_toggle_auto_levels(void);
bool post_pass_is_auto_levels_active(void);

// --- Рендер ---
void post_pass_render(GLuint source_tex, point_s dst, image_size_s size, image_size_s win);
