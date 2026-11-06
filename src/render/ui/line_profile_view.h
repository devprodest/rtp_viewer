#pragma once
#include "util/types.h"
#include <stdbool.h>

void line_profile_view_toggle(void);
bool line_profile_view_is_enabled(void);
void line_profile_view_draw(image_size_s win, float dpi_scale);
