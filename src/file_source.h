#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "rtp.h"

/// \file file_source.h
/// \brief Источник кадров из .raw-файла, совместимый по интерфейсу с rtp.c.
/// \date 2026
///
/// \details
/// Читает плоский поток кадров (W×H×bpp подряд) из файла. Подаёт их в темпе
/// mock_fps кадров в секунду. По достижении конца файла зацикливается.
/// Используется для отладки без живого потока и для воспроизведения
/// записанных сессий.
///
/// Состояние кадра rtp_frame_state_s переиспользуется как есть — поля
/// last_seq, lost_packets, packet_count не имеют смысла для файла, но
/// ничего не ломают.

/// \brief Открыть .raw-файл как источник кадров.
///
/// \param path           Путь к файлу
/// \param width          Ширина кадра (пиксели)
/// \param height         Высота кадра (пиксели)
/// \param bytes_per_pixel Размер пикселя
/// \param mock_fps       Кадров в секунду (1..1000). 0 → 25.
///
/// \return true при успехе
///
/// \warning Размер файла должен быть кратен width*height*bytes_per_pixel.
///          Хвост неполного кадра игнорируется.
bool file_source_open(const char * path, int width, int height, int bytes_per_pixel, int mock_fps);

/// \brief Закрыть файл.
void file_source_close(void);

/// \brief Получить указатель на актуальный кадр (если он готов).
///
/// \details Семантика совпадает с rtp_receive_packets: возвращает число
///          «пакетов» (0 или 1 — для файла это «кадр подан»), и пишет кадр
///          в buffer. Клавиша pause / timeout обрабатываются снаружи.
///
/// \param state  Состояние кадра (используется как признак «готов кадр»)
/// \param buffer Буфер назначения (frame_size байт)
///
/// \return 1 если кадр отдан, 0 если ещё рано, -1 при ошибке чтения
int file_source_receive_frame(rtp_frame_state_s * state, uint8_t * buffer);

/// \brief Проверить таймаут (совместимо с rtp_frame_check_timeout).
///
/// \details Для файла всегда false — кадры приходят по расписанию.
///
/// \return Всегда false
bool file_source_check_timeout(rtp_frame_state_s * state);
