#pragma once

#include "util/types.h"

point_s stats_overlay_get_pos(void);
void stats_overlay_set_pos(point_s pos);

image_size_s stats_overlay_get_size(float dpi_scale);

/// \brief Рисует панель статистики по заданной позиции.
///        Вызывается внутри overlay_begin/overlay_end.
void stats_overlay_draw(point_s pos, float dpi_scale);
