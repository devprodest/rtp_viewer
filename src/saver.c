// saver.c - сохранение кадров

#include "saver.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

// Буфер накопленных кадров
typedef struct frame_buffer_s
{
    uint8_t ** frames;
    int count;
    int capacity;
    int frame_size;
} frame_buffer_s;

static struct
{
    bool enabled;
    bool recording;
    char prefix[256];
    char outdir[256];
    int batch_size;
    int time_window;
    int file_counter;
    time_t last_save_time;
    frame_buffer_s buffer;

    // Время начала накопления текущего батча
    time_t batch_start_time;

    // Для имени файла
    int width;
    int height;
    char format_name[64];
} g_saver;

void saver_init(const char * prefix, const char * outdir, int batch_size, int time_window, int width, int height,
                int bytes_per_pixel, const char * format_name)
{
    memset(&g_saver, 0, sizeof(g_saver));

    g_saver.enabled = true;

    if (prefix && *prefix)
    {
        strncpy(g_saver.prefix, prefix, sizeof(g_saver.prefix) - 1);
        g_saver.recording = true;
    }
    else
    {
        strcpy(g_saver.prefix, "rtp");
        g_saver.recording = false;
    }

    if (outdir)
    {
        strncpy(g_saver.outdir, outdir, sizeof(g_saver.outdir) - 1);
    }

    g_saver.batch_size       = batch_size > 0 ? batch_size : 1;
    g_saver.time_window      = time_window;
    g_saver.file_counter     = 0;
    g_saver.last_save_time   = time(NULL);
    g_saver.batch_start_time = 0;

    g_saver.width  = width;
    g_saver.height = height;
    if (format_name)
    {
        strncpy(g_saver.format_name, format_name, sizeof(g_saver.format_name) - 1);
    }

    g_saver.buffer.frames     = NULL;
    g_saver.buffer.count      = 0;
    g_saver.buffer.capacity   = 0;
    g_saver.buffer.frame_size = width * height * bytes_per_pixel;
}

static void ensure_capacity(frame_buffer_s * buf, int need)
{
    if (buf->capacity >= need)
    {
        return;
    }

    int new_cap = buf->capacity ? buf->capacity * 2 : 16;
    if (new_cap < need)
    {
        new_cap = need;
    }

    uint8_t ** nf = realloc(buf->frames, (size_t)new_cap * sizeof(uint8_t *));
    if (!nf)
    {
        perror("realloc");
        exit(1);
    }
    buf->frames   = nf;
    buf->capacity = new_cap;
}

static bool has_outdir(void)
{
    return g_saver.outdir[0] != '\0' && strcmp(g_saver.outdir, ".") != 0;
}

// Имя файла: prefix_YYYYMMDD_HHMMSS_WxH_format_NNNN.raw
// Время берётся из g_saver.batch_start_time - момента начала накопления батча.
static void build_filename(char * out, size_t outsz)
{
    time_t t           = g_saver.batch_start_time ? g_saver.batch_start_time : time(NULL);
    struct tm * tm_now = localtime(&t);

    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", tm_now);

    if (has_outdir())
    {
        snprintf(out, outsz, "%s/%s_%s_%dx%d_%s_%04d.raw", g_saver.outdir, g_saver.prefix, stamp, g_saver.width,
                 g_saver.height, g_saver.format_name[0] ? g_saver.format_name : "unknown", g_saver.file_counter);
    }
    else
    {
        snprintf(out, outsz, "%s_%s_%dx%d_%s_%04d.raw", g_saver.prefix, stamp, g_saver.width, g_saver.height,
                 g_saver.format_name[0] ? g_saver.format_name : "unknown", g_saver.file_counter);
    }
}

static void save_buffer(frame_buffer_s * buf)
{
    if (buf->count == 0)
    {
        return;
    }

    if (has_outdir())
    {
        struct stat st;
        if (stat(g_saver.outdir, &st) != 0)
        {
#ifdef _WIN32
            mkdir(g_saver.outdir);
#else
            mkdir(g_saver.outdir, 0755);
#endif
        }
    }

    g_saver.file_counter++;

    char filename[512];
    build_filename(filename, sizeof(filename));

    FILE * f = fopen(filename, "wb");
    if (!f)
    {
        perror("fopen save");
        return;
    }

    for (int i = 0; i < buf->count; i++)
    {
        fwrite(buf->frames[i], 1, (size_t)buf->frame_size, f);
        free(buf->frames[i]);
        buf->frames[i] = NULL;
    }
    fclose(f);

    printf("Saved %s (frames=%d, size=%d bytes)\n", filename, buf->count, buf->count * buf->frame_size);

    buf->count             = 0;
    g_saver.last_save_time = time(NULL);

    // Сброс: следующий батч начнёт своё время с первого кадра
    g_saver.batch_start_time = 0;
}

void saver_add_frame(const uint8_t * frame_data)
{
    if (!g_saver.enabled || !g_saver.recording)
    {
        return;
    }

    // Фиксируем время начала батча на самом первом кадре
    if (g_saver.buffer.count == 0)
    {
        g_saver.batch_start_time = time(NULL);
    }

    ensure_capacity(&g_saver.buffer, g_saver.buffer.count + 1);

    uint8_t * copy = malloc((size_t)g_saver.buffer.frame_size);
    if (!copy)
    {
        return;
    }

    memcpy(copy, frame_data, (size_t)g_saver.buffer.frame_size);
    g_saver.buffer.frames[g_saver.buffer.count++] = copy;

    bool should_save = false;

    if (g_saver.buffer.count >= g_saver.batch_size)
    {
        should_save = true;
    }

    if (g_saver.time_window > 0)
    {
        time_t now = time(NULL);
        if (difftime(now, g_saver.last_save_time) >= (double)g_saver.time_window)
        {
            should_save = true;
        }
    }

    if (should_save)
    {
        save_buffer(&g_saver.buffer);
    }
}

void saver_flush(void)
{
    if (g_saver.enabled && g_saver.buffer.count > 0)
    {
        save_buffer(&g_saver.buffer);
    }
}

void saver_toggle_recording(void)
{
    if (!g_saver.enabled)
    {
        return;
    }

    g_saver.recording = !g_saver.recording;
    printf("Recording %s\n", g_saver.recording ? "enabled" : "disabled");

    if (!g_saver.recording && g_saver.buffer.count > 0)
    {
        saver_flush();
    }
}

void saver_cleanup(void)
{
    if (g_saver.buffer.count > 0)
    {
        save_buffer(&g_saver.buffer);
    }

    for (int i = 0; i < g_saver.buffer.count; i++)
    {
        if (g_saver.buffer.frames[i])
        {
            free(g_saver.buffer.frames[i]);
        }
    }
    free(g_saver.buffer.frames);
    g_saver.buffer.frames   = NULL;
    g_saver.buffer.count    = 0;
    g_saver.buffer.capacity = 0;
}

bool saver_is_recording(void)
{
    return g_saver.recording;
}
