#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "analysis/selection.h"

typedef struct flicker_metrics_s
{
    double cur;
    double min;
    double max;
    double mean;
    double std;
    double peak_to_peak;
    int count;
    bool valid;
} flicker_metrics_s;

/// \brief Читает feature.flicker.window (по умолчанию 300).
void flicker_init(void);
void flicker_reset(void);

/// \brief Считает mean-luma по текущему кадру из frame_context
///        (по ROI, если есть выделение) и кладёт в ring buffer.
///        Требует предварительно вызванного frame_context_set().
void flicker_update(const selection_s * sel);

flicker_metrics_s flicker_get_metrics(void);

/// \brief Рисует график mean-luma в RGBA-буфер w×h. Нормировка по окну
///        считается внутри.
void flicker_render_rgba(uint8_t * rgba, int w, int h);
