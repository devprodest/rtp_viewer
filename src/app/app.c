#include "app.h"

#include "config.h"
#include "control/hotkeys.h"
#include "control/mouse.h"
#include "log/logger.h"
#include "log/stats.h"
#include "render/renderer.h"
#include "render/ui/progress_bar.h"
#include "render/ui/svg_icons.h"
#include "render/ui/window_title.h"
#include "save/saver.h"
#include "source/source.h"
#include "source/source_factory.h"
#include "util/utils.h"
#include "util/version.h"

#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <mmsystem.h>
    #include <windows.h>
    #define SLEEP_US(us) Sleep((us) >= 1000 ? ((DWORD)((us) / 1000)) : 1)
#else
    #include <unistd.h>
    #define SLEEP_US(us) usleep(us)
#endif

static volatile bool g_running    = true;
static const app_config_s * g_cfg = nullptr;

//------------------------------------------------------- Синхронизация ------------------------------------------------

static pthread_mutex_t g_frame_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

static uint8_t * g_worker_buffer = nullptr; // Сюда читает воркер
static uint8_t * g_render_buffer = nullptr; // Общий буфер (под мьютексом)
static uint8_t * g_main_buffer   = nullptr; // Сюда копирует главный поток для рендера
static size_t g_frame_bytes      = 0;

static bool g_has_new_frame = false;

static source_s * g_source        = nullptr;
static bool g_worker_paused       = false;
static int g_worker_step_pending  = 0;
static bool g_worker_seek_pending = false;
static int g_worker_seek_target   = 0;
static int g_worker_cur_frame     = 0;
static int g_worker_tot_frames    = 0;

static bool g_scrubbing = false;

static void signal_handler(int /* unused */)
{
    g_running = false;
}

//------------------------------------------------------- Управление (Main Thread)
//--------------------------------------

bool app_toggle_pause(void)
{
    pthread_mutex_lock(&g_state_mutex);
    g_worker_paused = !g_worker_paused;
    bool paused     = g_worker_paused;
    pthread_mutex_unlock(&g_state_mutex);

    renderer_set_paused(paused);
    return paused;
}

bool app_is_paused(void)
{
    pthread_mutex_lock(&g_state_mutex);
    bool paused = g_worker_paused;
    pthread_mutex_unlock(&g_state_mutex);
    return paused;
}

bool app_step_forward(void)
{
    if (!g_cfg || !g_cfg->use_file_source || !g_source)
    {
        return false;
    }
    if (!app_is_paused())
    {
        app_toggle_pause();
    }
    pthread_mutex_lock(&g_state_mutex);
    g_worker_step_pending = +1;
    pthread_mutex_unlock(&g_state_mutex);
    return true;
}

bool app_step_backward(void)
{
    if (!g_cfg || !g_cfg->use_file_source || !g_source)
    {
        return false;
    }
    if (!app_is_paused())
    {
        app_toggle_pause();
    }
    pthread_mutex_lock(&g_state_mutex);
    g_worker_step_pending = -1;
    pthread_mutex_unlock(&g_state_mutex);
    return true;
}

bool app_seek_to(int target_frame)
{
    if (!g_cfg || !g_cfg->use_file_source || !g_source)
    {
        return false;
    }

    int tot = 0;
    pthread_mutex_lock(&g_state_mutex);
    tot = g_worker_tot_frames;
    pthread_mutex_unlock(&g_state_mutex);

    if (tot <= 0)
    {
        return false;
    }

    target_frame = utils_clamp(target_frame, 0, tot - 1);

    pthread_mutex_lock(&g_state_mutex);
    g_worker_seek_pending = true;
    g_worker_seek_target  = target_frame;
    pthread_mutex_unlock(&g_state_mutex);
    return true;
}

bool app_seek_fraction(float fraction)
{
    if (!g_cfg || !g_cfg->use_file_source || !g_source)
    {
        return false;
    }

    int tot = 0;
    pthread_mutex_lock(&g_state_mutex);
    tot = g_worker_tot_frames;
    pthread_mutex_unlock(&g_state_mutex);

    if (tot <= 0)
    {
        return false;
    }

    fraction   = utils_clamp(fraction, 0.0f, 1.0f);
    int target = (int)(fraction * (float)(tot - 1) + 0.5f);
    return app_seek_to(target);
}

//------------------------------------------------------- Worker Thread -----------------------------------------------

static void * worker_thread(void * arg)
{
    source_s * src = (source_s *)arg;
    frame_state_s state;
    frame_state_init(&state, g_cfg->image, (uint16_t)renderer_get_bytes_per_pixel());

    while (g_running)
    {
        int step  = 0;
        bool seek = false, paused = false;
        int target = 0;

        pthread_mutex_lock(&g_state_mutex);
        step   = g_worker_step_pending;
        seek   = g_worker_seek_pending;
        target = g_worker_seek_target;
        paused = g_worker_paused;
        pthread_mutex_unlock(&g_state_mutex);

        bool step_this_iter = false;

        if (seek)
        {
            source_seek_to(src, target);
            pthread_mutex_lock(&g_state_mutex);
            g_worker_seek_pending = false;
            pthread_mutex_unlock(&g_state_mutex);
            step           = 1; // Принудительно вычитываем один кадр после seek
            step_this_iter = true;
        }

        if (step != 0)
        {
            if (step > 0)
            {
                source_step_forward(src);
            }
            else
            {
                source_step_backward(src);
            }

            pthread_mutex_lock(&g_state_mutex);
            g_worker_step_pending = 0;
            pthread_mutex_unlock(&g_state_mutex);
            step_this_iter = true;
        }

        const bool skip_receive = paused && g_cfg->use_file_source && !step_this_iter;

        int received = 0;
        if (!skip_receive)
        {
            received = source_receive(src, &state, g_worker_buffer);
            if (received < 0)
            {
                log_error("app: source receive failed\n");
                break;
            }
            if (received > 0 && !g_cfg->use_file_source)
            {
                stats_add_packets(received);
            }
        }

        bool frame_ready = g_cfg->use_file_source ? (received == 1)
                                                  : (frame_state_is_ready(&state) || source_check_timeout(src, &state));

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
            if (source_get_frame_position(src, &cur, &tot))
            {
                pthread_mutex_lock(&g_state_mutex);
                g_worker_cur_frame  = cur;
                g_worker_tot_frames = tot;
                pthread_mutex_unlock(&g_state_mutex);
            }
        }
        else
        {
            SLEEP_US(1000); // 1ms, если кадр ещё не собран
        }
    }
    return nullptr;
}

//------------------------------------------------------- Main ---------------------------------------------------------

static void cleanup_buffers(void)
{
    free(g_worker_buffer);
    g_worker_buffer = nullptr;
    free(g_render_buffer);
    g_render_buffer = nullptr;
    free(g_main_buffer);
    g_main_buffer = nullptr;
}

int app_run(const app_config_s * cfg)
{
    g_cfg = cfg;

    log_info("%s v%s", RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR);

    stats_init();
    window_title_init(cfg->format, cfg->image);

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    if (!renderer_init(cfg->image, cfg->format, cfg->effective_bits, cfg->smooth_start))
    {
        log_error("app: Failed to initialize OpenGL\n");
        stats_cleanup();
        return EXIT_FAILURE;
    }

    if (!svg_icons_init())
    {
        log_error("app: Failed to initialize NanoVG");
    }

    int bpp = renderer_get_bytes_per_pixel();

    saver_init(&(saver_config_s) {
        .prefix      = cfg->save_prefix,
        .outdir      = cfg->outdir,
        .batch_size  = cfg->batch_size,
        .time_window = cfg->time_window,
        .image       = cfg->image,
        .bpp         = bpp,
        .format_name = cfg->format,
    });

    window_title_refresh();

    source_params_s src_params = {
        .image    = cfg->image,
        .bpp      = bpp,
        .port     = cfg->port,
        .path     = cfg->source_file,
        .mock_fps = cfg->mock_fps,
    };

    source_s * src = source_create(&src_params);
    if (!source_open(src, &src_params))
    {
        log_error("app: Failed to open data source\n");
        saver_cleanup();
        renderer_cleanup();
        stats_cleanup();
        return EXIT_FAILURE;
    }

    g_source = src;

    signal(SIGINT, signal_handler);

    g_frame_bytes = cfg->image.x * cfg->image.y * bpp;

    g_worker_buffer = (uint8_t *)calloc(1, g_frame_bytes);
    g_render_buffer = (uint8_t *)calloc(1, g_frame_bytes);
    g_main_buffer   = (uint8_t *)calloc(1, g_frame_bytes);

    if (!g_worker_buffer || !g_render_buffer || !g_main_buffer)
    {
        log_error("app: Failed to allocate frame buffers\n");
        cleanup_buffers();
        source_close(src);
        renderer_cleanup();
        stats_cleanup();
        return EXIT_FAILURE;
    }

    GLFWwindow * window = renderer_get_window();
    mouse_state_init(window);

    // Запускаем воркер чтения данных
    pthread_t thread;
    if (pthread_create(&thread, nullptr, worker_thread, src) != 0)
    {
        log_error("app: Failed to create worker thread\n");
        cleanup_buffers();
        source_close(src);
        renderer_cleanup();
        stats_cleanup();
        return EXIT_FAILURE;
    }

    mouse_state_s mouse;

    while (g_running && !glfwWindowShouldClose(window))
    {
        renderer_poll_events();
        mouse_state_update(window, &mouse);

        double wheel = mouse_state_take_wheel(&mouse);

        point_s mouse_point = {mouse.x, mouse.y};
        if (wheel != 0.0)
        {
            renderer_zoom_apply_wheel(wheel, mouse_point);
        }
        renderer_pan_apply_drag(mouse_point, mouse.middle);

        hotkeys_process(window);

        bool menu_consumed = renderer_handle_context_menu(mouse_point, mouse.right, mouse.left);

        // Progress bar
        bool progress_consumed = false;
        if (!menu_consumed && cfg->use_file_source)
        {
            float frac  = 0.0f;
            bool on_bar = progress_bar_hit_test(mouse_point, renderer_get_dpi_scale(), &frac);
            if (mouse.left && (on_bar || g_scrubbing))
            {
                app_seek_fraction(frac);
                g_scrubbing       = true;
                progress_consumed = true;
            }
            else if (!mouse.left)
            {
                g_scrubbing = false;
            }
        }

        bool input_consumed = menu_consumed || progress_consumed;
        renderer_set_mouse_pos(mouse_point);

        if (!input_consumed)
        {
            point_s frame = {0};
            bool in_view  = renderer_screen_to_frame(mouse_point, &frame);
            renderer_set_selection_from_mouse(frame, mouse.left, in_view);
        }

        // --- ЗАБИРАЕМ НОВЫЙ КАДР ИЗ ВОРКЕРА ---
        bool new_frame = false;
        pthread_mutex_lock(&g_frame_mutex);
        if (g_has_new_frame)
        {
            memcpy(g_main_buffer, g_render_buffer, g_frame_bytes);
            g_has_new_frame = false;
            new_frame       = true;
        }
        pthread_mutex_unlock(&g_frame_mutex);

        if (new_frame)
        {
            renderer_set_frame(g_main_buffer);
        }

        // --- ОБНОВЛЯЕМ UI ---
        int cur = 0, tot = 0;
        pthread_mutex_lock(&g_state_mutex);
        cur = g_worker_cur_frame;
        tot = g_worker_tot_frames;
        pthread_mutex_unlock(&g_state_mutex);

        if (tot > 0)
        {
            window_title_set_frame(cur, tot);
            progress_bar_set(cur, tot);
        }

        // --- РИСУЕМ ВСЕГДА (плавный UI) ---
        renderer_draw();

        stats_update();
    }

    g_running = false;
    pthread_join(thread, nullptr); // Ждём завершения воркера

    g_source = nullptr;
    cleanup_buffers();

    saver_cleanup();
    stats_cleanup();
    svg_icons_cleanup();
    renderer_cleanup();
    source_close(src);

#ifdef _WIN32
    timeEndPeriod(1);
#endif
    return EXIT_SUCCESS;
}
