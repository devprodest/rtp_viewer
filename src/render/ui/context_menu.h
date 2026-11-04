#pragma once

#include <stdbool.h>

#include "control/commands.h"
#include "util/types.h"

void context_menu_open(point_s pos);
void context_menu_close(void);
bool context_menu_is_open(void);

void context_menu_set_hover(point_s mouse);
void context_menu_draw(image_size_s win, float scale);

/// \brief Возвращает true, если клик был обработан меню (и меню закрылось).
bool context_menu_handle_click(point_s mouse, image_size_s win, float scale);

void context_menu_draw_icons(image_size_s win, float scale);
