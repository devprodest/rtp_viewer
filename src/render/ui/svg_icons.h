#pragma once

#include "control/commands.h"
#include "util/types.h"

/// \brief Инициализирует NanoVG. Вызывать один раз после создания окна.
bool svg_icons_init(void);
void svg_icons_cleanup(void);

/// \brief Начинает кадр NanoVG. Должен быть вызван до всех svg_icons_draw.
void svg_icons_begin(image_size_s win, float dpi_scale);
void svg_icons_end(void);

/// \brief Рисует SVG-иконку для команды. Если для команды нет SVG,
///        возвращает false (можно нарисовать fallback-примитивами).
bool svg_icons_draw(commands_e cmd, point_s center, float size, fcolor_rgba_s color);

/// \brief Проверяет, есть ли SVG-иконка для этой команды.
bool svg_icon_exists(commands_e cmd);
