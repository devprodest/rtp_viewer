#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "util/types.h"

void diff_view_init(void);
void diff_view_cleanup(void);

// --- Управление ---
void diff_view_toggle(void);
bool diff_view_is_enabled(void);
void diff_view_disable(void);

void diff_view_set_reference(void);
void diff_view_gain_up(void);
void diff_view_gain_down(void);

// --- Обработка ---
const uint8_t * diff_view_process(const uint8_t * data, image_size_s size, int bpp, int effective_bits);
