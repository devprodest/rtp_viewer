#pragma once

#include <stdbool.h>

#include "gl_api.h"

bool post_pass_init(GLuint quad_vao);
void post_pass_cleanup(void);

void post_pass_render(GLuint source_tex, int dst_x, int dst_y, int dst_w, int dst_h);
void post_pass_set_saturation(bool enabled);

void post_pass_set_gradient(bool enabled);
void post_pass_set_texel_size(float texel_x, float texel_y);
