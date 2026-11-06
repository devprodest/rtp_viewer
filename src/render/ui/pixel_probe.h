#pragma once

#include "util/types.h"
#include <stdint.h>

/// \brief Включает/выключает режим pixel probe (перекрестие + панель у курсора).
void pixel_probe_toggle(void);
bool pixel_probe_is_enabled(void);

/// \brief Передаёт актуальный кадр из renderer.c. Вызывается каждый кадр,
///        пока probe выключен — тоже (чтобы при включении сразу работало).
void pixel_probe_set_frame(const uint8_t * data, image_size_s frame_size, int bbp);

/// \brief Передаёт позицию курсора в экранных (framebuffer) координатах.
void pixel_probe_set_mouse(point_s screen);

/// \brief Рисует перекрестие и панель. Вызывается внутри overlay-контекста
///        (между overlay_begin и overlay_end).
void pixel_probe_draw(image_size_s win, float dpi_scale);
