#pragma once

#include "source/source.h"
#include "util/types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// \brief Запускает поток чтения источника и тройную буферизацию.
///        frame_bytes — размер одного кадра в байтах.
bool pipeline_start(source_s * src, image_size_s image, int bpp, size_t frame_bytes, bool is_file_source);

/// \brief Останавливает поток, ждёт join и освобождает буферы.
void pipeline_stop(void);

/// \brief Забирает последний готовый кадр (копирование в out_buf).
///        Возвращает true, если был новый кадр с прошлого вызова.
bool pipeline_take_frame(uint8_t * out_buf, size_t buf_size);

/// \brief Пауза / резюме. Возвращает новое состояние.
bool pipeline_toggle_pause(void);
bool pipeline_is_paused(void);

void pipeline_step_forward(void);
void pipeline_step_backward(void);

bool pipeline_seek_to(int target);
bool pipeline_seek_fraction(float fraction);

/// \brief Позиция в файле. Возвращает false, если источник не файловый.
bool pipeline_get_position(int * cur, int * tot);
