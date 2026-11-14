#pragma once

#include <stdint.h>

void screenshot_init(void);
void screenshot_cleanup(void);

void screenshot_request(void);
void screenshot_process(void);

void screenshot_size_set(int width, int height);