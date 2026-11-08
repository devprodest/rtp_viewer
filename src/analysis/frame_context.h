#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "format/format_config.h"
#include "util/types.h"

/// \brief Полностью декодированный пиксель кадра.
///
/// \details Один раз декодированный — используется всеми модулями (stats,
///          инспектор, line profile, детектор дефектов). Значения гарантированно
///          совпадают между модулями, потому что источник один.
typedef struct frame_pixel_s
{
    uint32_t raw;    ///< Сырое значение (0..255 для 8-bit, 0..65535 для 16-bit, 0 для RGB)
    uint8_t r, g, b; ///< Нормализованные 8-бит компоненты (для YCbCr — байты как есть: Y, Cb, Cr)
    uint8_t luma;    ///< Яркость 0..255 (для YCbCr — Y, для RGB — формула BT.601)
} frame_pixel_t;

/// \brief Устанавливает текущий кадр. Заменяет старую `pixel_probe_set_frame`.
void frame_context_set(const uint8_t * data, image_size_s size, const format_config_s * fmt, int effective_bits);

/// \brief Сбрасывает контекст (например, при очистке).
void frame_context_clear(void);

// --- Геттеры ---
const uint8_t * frame_context_data(void);
image_size_s frame_context_size(void);
int frame_context_bpp(void);
int frame_context_bits(void);
const format_config_s * frame_context_format(void);
bool frame_context_is_valid(void);

/// \brief Декодирует пиксель. Возвращает нули при выходе за границы.
frame_pixel_t frame_context_read(int x, int y);
