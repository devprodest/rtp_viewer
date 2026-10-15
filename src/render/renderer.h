#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

bool renderer_init(int width, int height, const char * format_name, int effective_bits, bool smooth_start);
void renderer_cleanup(void);

GLFWwindow * renderer_get_window(void);
void renderer_poll_events(void);
void renderer_update_texture(const uint8_t * data);

void renderer_refresh_title(void);

int renderer_get_bytes_per_pixel(void);

void renderer_set_demosaic_mode(int mode);
void renderer_set_paused(bool paused);

void renderer_toggle_fullscreen(void);
void renderer_toggle_smoothing(void);
void renderer_toggle_histogram(void);
void renderer_toggle_histogram_scale(void);
void renderer_toggle_histogram_cdf(void);
void renderer_toggle_saturation(void);

void renderer_request_screenshot(void);


void renderer_set_selection_from_mouse(int frame_x, int frame_y, bool left_down, bool right_down, bool in_view);
void renderer_clear_selection(void);
bool renderer_screen_to_frame(double mx, double my, int * fx, int * fy);
