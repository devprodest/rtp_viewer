#pragma once

#include <stdint.h>

#include "format/format_config.h"
#include "gl_api.h"

bool format_pass_init(int width, int height, GLuint quad_vao, int effective_bits, const format_config_s * cfg);
void format_pass_cleanup(void);

void format_pass_upload(const uint8_t * data);
void format_pass_render_to_fbo(void);
void format_pass_render_to_bound_fbo(void);

GLuint format_pass_get_output_tex(void);
int format_pass_get_bytes_per_pixel(void);
