#pragma once

#include <stdbool.h>

#include "util/types.h"

/// \brief Обновляет текущую позицию и общее число кадров.
///        Вызывается из app.c при каждом новом кадре / после seek.
void progress_bar_set(int current, int total);

/// \brief Сообщает модулю текущую позицию курсора. Должна вызываться
///        каждый кадр (из renderer_set_mouse_pos). Нужна для автоскрытия.
void progress_bar_notify_mouse(point_s mouse);

/// \brief Рисует полосу внизу окна. Вызывается из renderer.c
///        внутри уже открытого overlay-контекста (overlay_begin...overlay_end).
void progress_bar_draw(image_size_s win, float dpi_scale);

/// \brief Проверяет попадание курсора в полосу. Возвращает true, если
///        курсор в области полосы (с небольшим допуском по Y), и пишет
///        в out_fraction нормализованную позицию по X (0..1).
bool progress_bar_hit_test(point_s screen, float dpi_scale, float * out_fraction);
