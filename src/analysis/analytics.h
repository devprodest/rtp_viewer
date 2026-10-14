#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "analysis/histogram.h"
#include "analysis/selection.h"
#include "format/format_config.h"

void analytics_update(const uint8_t * data, int width, int height, const format_config_s * fmt, const char * format_name,
                      const selection_s * sel);

const histogram_s * analytics_get_histogram(void);
bool analytics_format_is_ycbcr(void);