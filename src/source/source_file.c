#include "source_file.h"

#include "util/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/// \brief FPS по умолчанию, если пользователь не задал.
#define FILE_SOURCE_DEFAULT_FPS 25

/// \brief Верхняя граница FPS (защита от абсурдных значений).
#define FILE_SOURCE_MAX_FPS     1000

/// \brief Нижняя граница FPS.
#define FILE_SOURCE_MIN_FPS     1

static struct
{
    FILE * file;           ///< Открытый файл
    size_t frame_size;     ///< Размер кадра в байтах
    size_t frames_total;   ///< Всего кадров в файле
    size_t frame_index;    ///< Номер следующего кадра
    uint64_t interval_us;  ///< Интервал между кадрами
    uint64_t next_send_us; ///< Время следующей отдачи
} g_file;

bool file_source_open(const char * path, int width, int height, int bytes_per_pixel, int mock_fps)
{
    file_source_close();

    FILE * f = fopen(path, "rb");
    if (!f)
    {
        perror("file_source: fopen");
        return false;
    }

    if (width <= 0 || height <= 0 || bytes_per_pixel <= 0)
    {
        fprintf(stderr, "file_source: invalid geometry\n");
        fclose(f);
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        perror("file_source: fseek");
        fclose(f);
        return false;
    }

    long len = ftell(f);
    if (len < 0)
    {
        perror("file_source: ftell");
        fclose(f);
        return false;
    }
    rewind(f);

    size_t frame_size = (size_t)width * (size_t)height * (size_t)bytes_per_pixel;
    if (frame_size == 0 || (size_t)len < frame_size)
    {
        fprintf(stderr, "file_source: file too small for one frame (%ld < %zu)\n", len, frame_size);
        fclose(f);
        return false;
    }

    size_t frames_total = (size_t)len / frame_size;

    int fps = mock_fps;
    if (fps < FILE_SOURCE_MIN_FPS)
    {
        fps = FILE_SOURCE_DEFAULT_FPS;
    }
    if (fps > FILE_SOURCE_MAX_FPS)
    {
        fps = FILE_SOURCE_MAX_FPS;
    }

    g_file.file         = f;
    g_file.frame_size   = frame_size;
    g_file.frames_total = frames_total;
    g_file.frame_index  = 0;
    g_file.interval_us  = 1000000ull / (uint64_t)fps;
    g_file.next_send_us = get_time_us();

    fprintf(stderr, "file_source: %s opened, %zu frames (%dx%d, %d bpp, %d fps)\n", path, frames_total, width, height,
            bytes_per_pixel, fps);
    return true;
}

void file_source_close(void)
{
    if (g_file.file)
    {
        fclose(g_file.file);
        g_file.file = NULL;
    }
    g_file.frame_size   = 0;
    g_file.frames_total = 0;
    g_file.frame_index  = 0;
    g_file.next_send_us = 0;
}

int file_source_receive_frame(rtp_frame_state_s * state, uint8_t * buffer)
{
    if (!g_file.file || !state || !buffer)
    {
        return -1;
    }

    uint64_t now = get_time_us();
    if (now < g_file.next_send_us)
    {
        return 0; // ещё рано
    }

    if (g_file.frame_index >= g_file.frames_total)
    {
        // Зацикливаемся
        rewind(g_file.file);
        g_file.frame_index = 0;
    }

    size_t got = fread(buffer, 1, g_file.frame_size, g_file.file);
    if (got != g_file.frame_size)
    {
        // Частичное чтение — либо конец файла, либо ошибка. Пробуем rewind один раз.
        rewind(g_file.file);
        g_file.frame_index = 0;
        got                = fread(buffer, 1, g_file.frame_size, g_file.file);
        if (got != g_file.frame_size)
        {
            fprintf(stderr, "file_source: short read (%zu / %zu)\n", got, g_file.frame_size);
            return -1;
        }
    }

    g_file.frame_index++;
    g_file.next_send_us = now + g_file.interval_us;

    // Обновляем поля состояния как при одном «пакете».
    state->received_bytes = (uint32_t)g_file.frame_size;
    state->packet_count++;
    state->last_packet_time_us = now;

    return 1;
}

bool file_source_check_timeout(rtp_frame_state_s * state)
{
    (void)state;
    return false;
}
