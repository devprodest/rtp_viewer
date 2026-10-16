#pragma once

#include "analysis/selection.h"
#include "format/format_config.h"
#include "util/types.h"

#include <stdint.h>

typedef struct sharpness_s
{
    double lap_var;   ///< Variance of Laplacian
    double tenengrad; ///< Сумма квадратов градиента
    int valid;        ///< 1, если удалось посчитать (был хотя бы один внутренний пиксель)
} sharpness_s;

sharpness_s sharpness_compute(const uint8_t * data, image_size_s win, const format_config_s * fmt,
                              const selection_s * sel);
