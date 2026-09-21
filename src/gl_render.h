#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gl_api.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

bool gl_render_init(int width, int height, const char * format_name, int effective_bits, bool smooth_start);
GLFWwindow * gl_render_get_window(void);
void gl_render_update_texture(const uint8_t * data);
void gl_render_poll_events(void);
void gl_render_cleanup(void);
void gl_render_toggle_smoothing(void);
void gl_render_set_demosaic_mode(int mode);
int gl_render_get_bytes_per_pixel(void);

void gl_render_refresh_title(void);
void gl_render_toggle_fullscreen(void);
