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
} g_saver;

void saver_init(const char * prefix, const char * outdir, int batch_size, int time_window, int width, int height,
                int bytes_per_pixel)
{
    memset(&g_saver, 0, sizeof(g_saver));

    if (!prefix)
    {
        g_saver.enabled = false;
        return;
    }

    g_saver.enabled   = true;
    g_saver.recording = true;

    strncpy(g_saver.prefix, prefix, sizeof(g_saver.prefix) - 1);
    g_saver.prefix[sizeof(g_saver.prefix) - 1] = '\0';

    if (outdir)
    {
        strncpy(g_saver.outdir, outdir, sizeof(g_saver.outdir) - 1);
    }

    g_saver.batch_size        = batch_size > 0 ? batch_size : 1;
    g_saver.time_window       = time_window;
    g_saver.file_counter      = 0;
    g_saver.last_save_time    = time(NULL);
    g_saver.buffer.frame_size = width * height * bytes_per_pixel;
    g_saver.buffer.frames     = NULL;
    g_saver.buffer.count      = 0;
    g_saver.buffer.capacity   = 0;
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

static void save_buffer(frame_buffer_s * buf)
{
    if (buf->count == 0)
    {
        return;
    }

    if (g_saver.outdir[0])
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

    char filename[1024];
    g_saver.file_counter++;
    if (g_saver.outdir[0])
    {
        snprintf(filename, sizeof(filename), "%s/%s_%08d.raw", g_saver.outdir, g_saver.prefix, g_saver.file_counter);
    }
    else
    {
        snprintf(filename, sizeof(filename), "%s_%04d.raw", g_saver.prefix, g_saver.file_counter);
    }

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

    printf("Saved file %s, frames=%d, size=%d bytes\n", filename, buf->count, buf->count * buf->frame_size);



    buf->count             = 0;
    g_saver.last_save_time = time(NULL);
}

void saver_add_frame(const uint8_t * frame_data)
{
    if (!g_saver.enabled || !g_saver.recording)
    {
        return;
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
}

void saver_cleanup(void)
{
    saver_flush();

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
