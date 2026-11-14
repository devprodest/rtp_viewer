#pragma once

#include <stdint.h>

#include "util/types.h"

/// \brief YCbCr → RGB, BT.601 full range.
/// \details Y ∈ [0..255], Cb/Cr ∈ [0..255]. Выход clamp-нут в [0..255].
u8color_rgb_s color_ycbcr_to_rgb(int y, int cb, int cr);

/// \brief RGB → YCbCr, BT.601 full range.
/// \details Вход RGB ∈ [0..255]. Выход Y/Cb/Cr ∈ [0..255] в полях r/g/b.
u8color_rgb_s color_rgb_to_ycbcr(int r, int g, int b);

/// \brief RGB → luma, BT.601 full range. Результат в [0..255].
uint8_t color_rgb_to_luma601(int r, int g, int b);