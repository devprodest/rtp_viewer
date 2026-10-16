#pragma once

#include <stdbool.h>

#include "gl_api.h"
#include "util/types.h"

bool post_pass_init(GLuint quad_vao);
void post_pass_cleanup(void);

void post_pass_render(GLuint source_tex, point_s dst, image_size_s size);
void post_pass_set_saturation(bool enabled);

void post_pass_set_gradient(bool enabled);
void post_pass_set_texel_size(fimage_size_s size);
