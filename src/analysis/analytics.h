#pragma once

#include "analysis/histogram.h"
#include "analysis/selection.h"
#include "format/format_config.h"
#include "util/types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct analytics_stats_s
{
    double min[ 3 ];
    double max[ 3 ];
    double mean[ 3 ];
    double std[ 3 ];
    uint64_t count; /* число пикселей (не каналов) */
    int channels;   /* 1 для gray, 3 для RGB */
    bool valid;
} analytics_stats_s;

const analytics_stats_s * analytics_get_stats(void);

/* управление видимостью оверлея из hotkeys.c */
void analytics_toggle_stats_visible(void);
bool analytics_is_stats_visible(void);

void analytics_update(const uint8_t * data, image_size_s win, const format_config_s * fmt, const selection_s * sel,
                      int effective_bits);

const histogram_s * analytics_get_histogram(void);
bool analytics_format_is_ycbcr(void);
