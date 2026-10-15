#pragma once

#include "analysis/selection.h"
#include "format/format_config.h"

#include <stdint.h>

typedef struct sharpness_s
{
    double lap_var;   ///< Variance of Laplacian
    double tenengrad; ///< Сумма квадратов градиента
    int valid;        ///< 1, если удалось посчитать (был хотя бы один внутренний пиксель)
} sharpness_s;

void sharpness_compute(const uint8_t * data, int width, int height, const format_config_s * fmt,
                       const selection_s * sel, sharpness_s * out);
