#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "analysis/histogram.h"
#include "analysis/selection.h"
#include "format/format_config.h"
#include "util/types.h"

#define ANALYTICS_VS_BINS 256

typedef struct analytics_stats_s
{
    double min[ 3 ];
    double max[ 3 ];
    double mean[ 3 ];
    double std[ 3 ];
    uint64_t count;
    int channels;
    bool valid;
} analytics_stats_s;

typedef struct analytics_vectorscope_s
{
    uint32_t bins[ ANALYTICS_VS_BINS ][ ANALYTICS_VS_BINS ]; // [cr][cb]
    uint32_t max_count;
    bool valid;
} analytics_vectorscope_s;

// --- Основной проход ---
void analytics_update(const uint8_t * data, image_size_s win, const format_config_s * fmt, const selection_s * sel,
                      int effective_bits);

// --- Геттеры ---
const analytics_stats_s * analytics_get_stats(void);
const histogram_s * analytics_get_histogram(void);
const analytics_vectorscope_s * analytics_get_vectorscope(void);
bool analytics_format_is_ycbcr(void);

// --- Оверлей stats ---
void analytics_toggle_stats_visible(void);
bool analytics_is_stats_visible(void);
void analytics_disable_stats(void);
