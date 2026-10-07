#pragma once

#include <stdint.h>

#define HIST_BINS     256
#define HIST_CHANNELS 3

typedef struct histogram_s
{
    uint32_t bins[HIST_CHANNELS][HIST_BINS];
    uint32_t max_per_channel[HIST_CHANNELS]; // максимум по каждому каналу отдельно
    int      active_channels;                // 0 = не поддерживается, 1 = grayscale, 3 = RGB/YCbCr
} histogram_s;

void histogram_reset(histogram_s * h);
void histogram_add(histogram_s * h, int channel, uint32_t bin);
void histogram_finalize(histogram_s * h);