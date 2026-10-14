#pragma once

#include <stdbool.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

bool window_init(int width, int height, const char * title);
void window_cleanup(void);

GLFWwindow * window_get(void);

bool window_should_close(void);
void window_poll_events(void);
void window_swap_buffers(void);
void window_get_framebuffer_size(int * w, int * h);

void window_set_title(const char * title);
bool window_is_fullscreen(void);
void window_toggle_fullscreen(void);