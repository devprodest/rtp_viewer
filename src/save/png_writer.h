#pragma once

#include <stdbool.h>
#include <stdint.h>

// Записать PNG (RGBA8, строки сверху вниз).
// Возвращает true при успехе.
bool png_write(const char * path, const uint8_t * rgba, int width, int height);
