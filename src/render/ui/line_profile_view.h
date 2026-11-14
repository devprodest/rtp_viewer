#pragma once

#include <stdbool.h>

#include "util/types.h"

void line_profile_view_init(void);

/// \brief Включает / выключает режим line profile.
void line_profile_view_toggle(void);
bool line_profile_view_is_enabled(void);
void line_profile_view_disable(void);

/// \brief Рисует два профиля (левая полоса — срез по X, нижняя — срез по Y)
///        относительно текущей позиции курсора. Вызывается внутри overlay.
void line_profile_view_draw(image_size_s win, float dpi_scale);
