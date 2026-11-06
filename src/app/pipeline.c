#include "pipeline.h"

#include "log/logger.h"
#include "log/stats.h"
#include "save/saver.h"
#include "util/utils.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP_US(us) Sleep((us) >= 1000 ? ((DWORD)((us) / 1000)) : 1)
#else
    #include <unistd.h>
    #define SLEEP_US(us) usleep(us)
#endif

static pthread_mutex_t g_frame_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static source_s * g_src      = nullptr;
static image_size_s g_image  = {0, 0};
static int g_bpp             = 0;
static size_t g_frame_bytes  = 0;
static bool g_is_file_source = false;

static uint8_t * g_worker_buffer = nullptr;
static uint8_t * g_render_buffer = nullptr;
static bool g_has_new_frame      = false;

static bool g_paused       = false;
static int g_step_pending  = 0;
static bool g_seek_pending = false;
static int g_seek_target   = 0;
static int g_cur_frame     = 0;
static int g_tot_frames    = 0;

static volatile bool g_running = false;
static pthread_t g_thread      = 0;

//------------------------------------------------------- Worker -------------------------------------------------------

static void * worker_thread(void * arg)
{
    (void)arg;

    frame_state_s state;
    frame_state_init(&state, g_image, (uint16_t)g_bpp);

    while (g_running)
    {
        int step    = 0;
        bool seek   = false;
        bool paused = false;
        int target  = 0;

        pthread_mutex_lock(&g_state_mutex);
        step   = g_step_pending;
        seek   = g_seek_pending;
        target = g_seek_target;
        paused = g_paused;
        pthread_mutex_unlock(&g_state_mutex);

        bool step_this_iter = false;

        if (seek)
        {
            source_seek_to(g_src, target);
            pthread_mutex_lock(&g_state_mutex);
            g_seek_pending = false;
            pthread_mutex_unlock(&g_state_mutex);
            step           = 1;
            step_this_iter = true;
        }

        if (step != 0)
        {
            if (step > 0)
            {
                source_step_forward(g_src);
            }
            else
            {
                source_step_backward(g_src);
            }

            pthread_mutex_lock(&g_state_mutex);
            g_step_pending = 0;
            pthread_mutex_unlock(&g_state_mutex);
            step_this_iter = true;
        }

        const bool skip_receive = paused && g_is_file_source && !step_this_iter;

        int received = 0;
        if (!skip_receive)
        {
            received = source_receive(g_src, &state, g_worker_buffer);
            if (received < 0)
            {
                log_error("pipeline: source receive failed");
                break;
            }
            if (received > 0 && !g_is_file_source)
            {
                stats_add_packets(received);
            }
        }

        bool frame_ready =
            g_is_file_source ? (received == 1) : (frame_state_is_ready(&state) || source_check_timeout(g_src, &state));

        if (frame_ready)
        {
            pthread_mutex_lock(&g_frame_mutex);
            memcpy(g_render_buffer, g_worker_buffer, g_frame_bytes);
            g_has_new_frame = true;
            pthread_mutex_unlock(&g_frame_mutex);

            saver_add_frame(g_worker_buffer);
            stats_frame_rendered();
            frame_state_reset(&state);

            int cur = 0;
            int tot = 0;
            if (source_get_frame_position(g_src, &cur, &tot))
            {
                pthread_mutex_lock(&g_state_mutex);
                g_cur_frame  = cur;
                g_tot_frames = tot;
                pthread_mutex_unlock(&g_state_mutex);
            }
        }
        else
        {
            SLEEP_US(1000);
        }
    }

    return nullptr;
}

//------------------------------------------------------- API ---------------------------------------------------------

bool pipeline_start(source_s * src, image_size_s image, int bpp, size_t frame_bytes, bool is_file_source)
{
    if (!src || frame_bytes == 0)
    {
        return false;
    }

    g_src            = src;
    g_image          = image;
    g_bpp            = bpp;
    g_frame_bytes    = frame_bytes;
    g_is_file_source = is_file_source;

    g_worker_buffer = (uint8_t *)calloc(1, g_frame_bytes);
    g_render_buffer = (uint8_t *)calloc(1, g_frame_bytes);
    if (!g_worker_buffer || !g_render_buffer)
    {
        log_error("pipeline: failed to allocate buffers");
        free(g_worker_buffer);
        g_worker_buffer = nullptr;
        free(g_render_buffer);
        g_render_buffer = nullptr;
        return false;
    }

    g_has_new_frame = false;
    g_paused        = false;
    g_step_pending  = 0;
    g_seek_pending  = false;
    g_cur_frame     = 0;
    g_tot_frames    = 0;

    if (is_file_source)
    {
        int cur = 0;
        int tot = 0;
        if (source_get_frame_position(src, &cur, &tot))
        {
            g_cur_frame  = cur;
            g_tot_frames = tot;
        }
    }

    g_running = true;

    if (pthread_create(&g_thread, nullptr, worker_thread, nullptr) != 0)
    {
        log_error("pipeline: failed to create thread");
        g_running = false;
        free(g_worker_buffer);
        g_worker_buffer = nullptr;
        free(g_render_buffer);
        g_render_buffer = nullptr;
        return false;
    }

    return true;
}

void pipeline_stop(void)
{
    if (!g_running)
    {
        return;
    }
    g_running = false;
    if (g_thread)
    {
        pthread_join(g_thread, nullptr);
        g_thread = 0;
    }
    free(g_worker_buffer);
    g_worker_buffer = nullptr;
    free(g_render_buffer);
    g_render_buffer = nullptr;
    g_has_new_frame = false;
}

bool pipeline_take_frame(uint8_t * out_buf, size_t buf_size)
{
    if (!out_buf || buf_size < g_frame_bytes)
    {
        return false;
    }

    bool got = false;
    pthread_mutex_lock(&g_frame_mutex);
    if (g_has_new_frame)
    {
        memcpy(out_buf, g_render_buffer, g_frame_bytes);
        g_has_new_frame = false;
        got             = true;
    }
    pthread_mutex_unlock(&g_frame_mutex);
    return got;
}

bool pipeline_toggle_pause(void)
{
    pthread_mutex_lock(&g_state_mutex);
    g_paused = !g_paused;
    bool p   = g_paused;
    pthread_mutex_unlock(&g_state_mutex);
    return p;
}

bool pipeline_is_paused(void)
{
    pthread_mutex_lock(&g_state_mutex);
    bool p = g_paused;
    pthread_mutex_unlock(&g_state_mutex);
    return p;
}

void pipeline_step_forward(void)
{
    if (!g_is_file_source || !g_src)
    {
        return;
    }
    if (!pipeline_is_paused())
    {
        pipeline_toggle_pause();
    }
    pthread_mutex_lock(&g_state_mutex);
    g_step_pending = +1;
    pthread_mutex_unlock(&g_state_mutex);
}

void pipeline_step_backward(void)
{
    if (!g_is_file_source || !g_src)
    {
        return;
    }
    if (!pipeline_is_paused())
    {
        pipeline_toggle_pause();
    }
    pthread_mutex_lock(&g_state_mutex);
    g_step_pending = -1;
    pthread_mutex_unlock(&g_state_mutex);
}

bool pipeline_seek_to(int target)
{
    if (!g_is_file_source || !g_src)
    {
        return false;
    }

    int tot = 0;
    pthread_mutex_lock(&g_state_mutex);
    tot = g_tot_frames;
    pthread_mutex_unlock(&g_state_mutex);

    if (tot <= 0)
    {
        return false;
    }

    target = utils_clamp(target, 0, tot - 1);

    pthread_mutex_lock(&g_state_mutex);
    g_seek_pending = true;
    g_seek_target  = target;
    pthread_mutex_unlock(&g_state_mutex);
    return true;
}

bool pipeline_seek_fraction(float fraction)
{
    if (!g_is_file_source || !g_src)
    {
        return false;
    }

    int tot = 0;
    pthread_mutex_lock(&g_state_mutex);
    tot = g_tot_frames;
    pthread_mutex_unlock(&g_state_mutex);

    if (tot <= 0)
    {
        return false;
    }

    fraction   = utils_clamp(fraction, 0.0f, 1.0f);
    int target = (int)(fraction * (float)(tot - 1) + 0.5f);
    return pipeline_seek_to(target);
}

bool pipeline_get_position(int * cur, int * tot)
{
    if (!g_is_file_source)
    {
        return false;
    }
    pthread_mutex_lock(&g_state_mutex);
    if (cur)
    {
        *cur = g_cur_frame;
    }
    if (tot)
    {
        *tot = g_tot_frames;
    }
    pthread_mutex_unlock(&g_state_mutex);
    return true;
}
