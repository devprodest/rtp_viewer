#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "util/types.h"

typedef enum defect_type_e
{
    DEFECT_DEAD,  // всегда минимум
    DEFECT_STUCK, // всегда максимум
    DEFECT_HOT,   // резко отличается от соседей
} defect_type_e;

typedef struct defect_s
{
    point_s pos;
    defect_type_e type;
    uint32_t value;
    int32_t delta;
} defect_s;

void defect_view_init(void);
void defect_view_cleanup(void);

void defect_view_toggle(void);
bool defect_view_is_enabled(void);

/// \brief Пересчитать результаты по текущему кадру (берёт из pixel_probe).
void defect_view_rescan(void);

/// \brief Отрисовка маркеров и итоговой панели. Вызывается внутри overlay.
void defect_view_draw(image_size_s win, float dpi_scale);
