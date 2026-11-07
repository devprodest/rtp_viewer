#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "util/types.h"

bool renderer_init(image_size_s win, const char * format_name, int effective_bits, bool smooth_start);
void renderer_cleanup(void);

GLFWwindow * renderer_get_window(void);
void renderer_poll_events(void);
void renderer_set_frame(const uint8_t * data);
void renderer_draw(void);
void renderer_refresh_analytics(void);

int renderer_get_bytes_per_pixel(void);
float renderer_get_dpi_scale(void);

void renderer_set_demosaic_mode(int mode);
void renderer_set_paused(bool paused);

void renderer_toggle_fullscreen(void);
void renderer_toggle_smoothing(void);
bool renderer_is_smoothing_active(void);

void renderer_set_selection_from_mouse(point_s mouse, bool left_down, bool in_view);
void renderer_clear_selection(void);
bool renderer_screen_to_frame(point_s screen, point_s * frame);

void renderer_zoom_reset(void);
void renderer_zoom_apply_wheel(double wheel, point_s point);
void renderer_pan_apply_drag(point_s point, bool middle_down);

void renderer_set_mouse_pos(point_s mouse);

bool renderer_handle_context_menu(point_s mouse, bool right_down, bool left_down);
