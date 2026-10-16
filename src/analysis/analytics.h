#pragma once

#include "analysis/histogram.h"
#include "analysis/selection.h"
#include "format/format_config.h"
#include "util/types.h"

#include <stdbool.h>
#include <stdint.h>

void analytics_update(const uint8_t * data, image_size_s win, const format_config_s * fmt, const char * fname,
                      const selection_s * sel);

const histogram_s * analytics_get_histogram(void);
bool analytics_format_is_ycbcr(void);
