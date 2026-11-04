#include "source_file.h"

#include "log/logger.h"
#include "util/utils.h"

#include <stdio.h>
#include <stdlib.h>

#define FILE_SOURCE_DEFAULT_FPS 25
#define FILE_SOURCE_MAX_FPS     1000
#define FILE_SOURCE_MIN_FPS     1

typedef struct file_impl_s
{
    FILE * file;
    size_t frame_size;
    size_t frames_total;
    size_t frame_index;
    uint64_t interval_us;
    uint64_t next_send_us;
} file_impl_t;

static file_impl_t g_file_impl;
static source_s g_file_source;

static bool file_open(source_s * self, const source_params_s * params);
static void file_close(source_s * self);
static int file_receive(source_s * self, frame_state_s * state, uint8_t * buffer);
static bool file_check_timeout(source_s * self, frame_state_s * state);

static bool file_seek_to(source_s * self, int target_frame);
static bool file_step_forward(source_s * self);
static bool file_step_backward(source_s * self);
static bool file_get_frame_position(source_s * self, int * current, int * total);

static const source_ops_s g_file_ops = {
    .open               = file_open,
    .close              = file_close,
    .receive            = file_receive,
    .check_timeout      = file_check_timeout,
    .step_forward       = file_step_forward,
    .step_backward      = file_step_backward,
    .seek_to            = file_seek_to,
    .get_frame_position = file_get_frame_position,
};

static bool file_open(source_s * self, const source_params_s * params)
{
    file_impl_t * impl = (file_impl_t *)self->impl;

    file_close(self);

    FILE * f = fopen(params->path, "rb");
    if (!f)
    {
        perror("file_source: fopen");
        return false;
    }

    if (params->image.x <= 0 || params->image.y <= 0 || params->bpp <= 0)
    {
        log_error("file: file_source: invalid geometry\n");
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

    size_t frame_size = params->image.x * params->image.y * params->bpp;
    if (frame_size == 0 || (size_t)len < frame_size)
    {
        log_error("file: file_source: file too small for one frame (%ld < %zu)\n", len, frame_size);
        fclose(f);
        return false;
    }

    size_t frames_total = (size_t)len / frame_size;

    int fps = params->mock_fps;
    if (fps < FILE_SOURCE_MIN_FPS)
    {
        fps = FILE_SOURCE_DEFAULT_FPS;
    }
    if (fps > FILE_SOURCE_MAX_FPS)
    {
        fps = FILE_SOURCE_MAX_FPS;
    }

    impl->file         = f;
    impl->frame_size   = frame_size;
    impl->frames_total = frames_total;
    impl->frame_index  = 0;
    impl->interval_us  = 1000000ull / (uint64_t)fps;
    impl->next_send_us = get_time_us();

    log_info("file: %s opened, %zu frames (%dx%d, %d bpp, %d fps)", params->path, frames_total, params->image.x,
             params->image.y, params->bpp, fps);
    return true;
}

static void file_close(source_s * self)
{
    file_impl_t * impl = (file_impl_t *)self->impl;

    if (impl->file)
    {
        fclose(impl->file);
        impl->file = nullptr;
    }
    impl->frame_size   = 0;
    impl->frames_total = 0;
    impl->frame_index  = 0;
    impl->next_send_us = 0;
}

static int file_receive(source_s * self, frame_state_s * state, uint8_t * buffer)
{
    file_impl_t * impl = (file_impl_t *)self->impl;

    if (!impl->file || !state || !buffer)
    {
        return -1;
    }

    uint64_t now = get_time_us();
    if (now < impl->next_send_us)
    {
        return 0;
    }

    if (impl->frame_index >= impl->frames_total)
    {
        rewind(impl->file);
        impl->frame_index = 0;
    }

    size_t got = fread(buffer, 1, impl->frame_size, impl->file);
    if (got != impl->frame_size)
    {
        rewind(impl->file);
        impl->frame_index = 0;
        got               = fread(buffer, 1, impl->frame_size, impl->file);
        if (got != impl->frame_size)
        {
            log_error("file: file_source: short read (%zu / %zu)\n", got, impl->frame_size);
            return -1;
        }
    }

    impl->frame_index++;

    // Планируем от предыдущего next_send_us, а не от now: иначе каждое
    // опоздание polling'а (до 1 мс после timeBeginPeriod(1)) накапливается,
    // и средний fps уезжает вниз. Если отстали больше чем на интервал
    // (диск stall, пауза) — сбрасываем базу, чтобы не было вспышки догоняющих.
    impl->next_send_us += impl->interval_us;
    if (impl->next_send_us <= now)
    {
        impl->next_send_us = now + impl->interval_us;
    }

    state->received_bytes = (uint32_t)impl->frame_size;
    state->packet_count++;
    state->last_packet_time_us = now;

    return 1;
}

static bool file_check_timeout(source_s * /*unused*/, frame_state_s * /*unused*/)
{
    return false;
}

source_s * source_file_create(void)
{
    g_file_impl.file   = nullptr;
    g_file_source.ops  = &g_file_ops;
    g_file_source.impl = &g_file_impl;
    return &g_file_source;
}

static bool file_step_forward(source_s * self)
{
    file_impl_t * impl = (file_impl_t *)self->impl;

    if (!impl->file || impl->frames_total == 0)
    {
        return false;
    }

    impl->next_send_us = 0;
    return true;
}

static bool file_step_backward(source_s * self)
{
    file_impl_t * impl = (file_impl_t *)self->impl;

    if (!impl->file || impl->frames_total < 2)
    {
        return false;
    }

    if (impl->frame_index < 2)
    {
        impl->frame_index = impl->frames_total - 1;
    }
    else
    {
        impl->frame_index -= 2;
    }

    if (fseek(impl->file, (long)(impl->frame_index * impl->frame_size), SEEK_SET) != 0)
    {
        return false;
    }

    impl->next_send_us = 0;
    return true;
}

static bool file_seek_to(source_s * self, int target_frame)
{
    file_impl_t * impl = (file_impl_t *)self->impl;

    if (!impl->file || impl->frames_total == 0)
    {
        return false;
    }

    if (target_frame < 0)
    {
        target_frame = 0;
    }
    if ((size_t)target_frame >= impl->frames_total)
    {
        target_frame = (int)impl->frames_total - 1;
    }

    if (fseek(impl->file, (long)((size_t)target_frame * impl->frame_size), SEEK_SET) != 0)
    {
        log_error("file: file_source: seek fseek failed\n");
        return false;
    }

    // frame_index указывает на "следующий кадр для чтения".
    // После seek сразу за target_frame, а file_receive инкрементирует после чтения.
    impl->frame_index  = (size_t)target_frame;
    impl->next_send_us = 0;

    return true;
}

static bool file_get_frame_position(source_s * self, int * current, int * total)
{
    file_impl_t * impl = (file_impl_t *)self->impl;
    if (!impl->file || impl->frames_total == 0)
    {
        return false;
    }
    int shown = (int)impl->frame_index - 1;
    if (shown < 0)
    {
        shown = 0;
    }
    *current = shown;
    *total   = (int)impl->frames_total;
    return true;
}
