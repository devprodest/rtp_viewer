#pragma once

#include <stdint.h>

void screenshot_init(void);
void screenshot_cleanup(void);

void screenshot_request(void);
void screenshot_process_if_requested(void);

void screenshot_set_size(int width, int height);