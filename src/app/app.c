#include "app.h"

#include "config.h"
#include "control/hotkeys.h"
#include "control/mouse.h"
#include "log/logger.h"
#include "log/stats.h"
#include "render/renderer.h"
#include "render/ui/progress_bar.h"
#include "render/ui/window_title.h"
#include "save/saver.h"
#include "source/source.h"
#include "source/source_factory.h"
#include "util/utils.h"
#include "util/version.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <mmsystem.h>
    #include <windows.h>
    #define SLEEP_MS(ms) Sleep(ms)
    #define SLEEP_US(us) Sleep((us) >= 1000 ? ((DWORD)((us) / 1000)) : 1)
#else
    #include <unistd.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
    #define SLEEP_US(us) usleep(us)
#endif

#define NUM_FRAME_BUFFERS 2

static volatile bool g_running = true;

static uint8_t * g_frame_buffers[ NUM_FRAME_BUFFERS ] = {nullptr, nullptr};
static uint8_t * g_paused_buffer                      = nullptr;
static int g_last_rendered_idx                        = 0;
static bool g_has_rendered                            = false;
static size_t g_frame_bytes                           = 0;
static bool g_paused                                  = false;

static const app_config_s * g_cfg = nullptr;

static source_s * g_source              = nullptr;
static int g_step_pending               = 0;
static bool g_seek_pending              = false;
static bool g_scrubbing                 = false;
static uint64_t g_last_paused_render_us = 0;

static void signal_handler(int /* unused */)
{
    g_running = false;
}

bool app_toggle_pause(void)
{
    g_paused = !g_paused;
    if (g_paused && g_has_rendered && g_paused_buffer)
    {
        memcpy(g_paused_buffer, g_frame_buffers[ g_last_rendered_idx ], g_frame_bytes);
        g_last_paused_render_us = 0;
    }
    renderer_set_paused(g_paused);
    return g_paused;
}

bool app_is_paused(void)
{
    return g_paused;
}

bool app_step_forward(void)
{
    if (!g_cfg || !g_cfg->use_file_source || !g_source)
    {
        return false;
    }
    if (!g_paused)
    {
        app_toggle_pause();
    }
    g_step_pending = +1;
    return true;
}

bool app_step_backward(void)
{
    if (!g_cfg || !g_cfg->use_file_source || !g_source)
    {
        return false;
    }
    if (!g_paused)
    {
        app_toggle_pause();
    }
    g_step_pending = -1;
    return true;
}

bool app_seek_to(int target_frame)
{
    if (!g_cfg || !g_cfg->use_file_source || !g_source)
    {
        return false;
    }

    int cur = 0;
    int tot = 0;
    if (!source_get_frame_position(g_source, &cur, &tot) || tot <= 0)
    {
        return false;
    }

    if (target_frame < 0)  target_frame = 0;
    if (target_frame >= tot) target_frame = tot - 1;

    if (!source_seek_to(g_source, target_frame))
    {
        return false;
    }

    g_seek_pending = true;
    return true;
}

bool app_seek_fraction(float fraction)
{
    if (!g_cfg || !g_cfg->use_file_source || !g_source)
    {
        return false;
    }

    int cur = 0;
    int tot = 0;
    if (!source_get_frame_position(g_source, &cur, &tot) || tot <= 0)
    {
        return false;
    }

    if (fraction < 0.0f)
    {
        fraction = 0.0f;
    }
    if (fraction > 1.0f)
    {
        fraction = 1.0f;
    }

    int target = (int)(fraction * (float)(tot - 1) + 0.5f);
    return app_seek_to(target);
}

static void cleanup_buffers(void)
{
    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        free(g_frame_buffers[ i ]);
        g_frame_buffers[ i ] = nullptr;
    }
    free(g_paused_buffer);
    g_paused_buffer = nullptr;
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
        log_error("app: Не удалось инициализировать OpenGL\n");
        stats_cleanup();
        return EXIT_FAILURE;
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
        log_error("app: Не удалось открыть источник данных\n");
        saver_cleanup();
        renderer_cleanup();
        stats_cleanup();
        return EXIT_FAILURE;
    }

    g_source = src;

    int frame_cur = 0;
    int frame_tot = 0;
    if (source_get_frame_position(src, &frame_cur, &frame_tot))
    {
        window_title_set_frame(frame_cur, frame_tot);
        progress_bar_set(frame_cur, frame_tot);
    }

    signal(SIGINT, signal_handler);

    g_frame_bytes = cfg->image.x * cfg->image.y * bpp;

    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        g_frame_buffers[ i ] = calloc(1, g_frame_bytes);
        if (!g_frame_buffers[ i ])
        {
            perror("calloc frame buffer");
            cleanup_buffers();
            source_close(src);
            renderer_cleanup();
            stats_cleanup();
            return EXIT_FAILURE;
        }
    }

    g_paused_buffer = calloc(1, g_frame_bytes);
    if (!g_paused_buffer)
    {
        perror("calloc paused buffer");
        cleanup_buffers();
        source_close(src);
        renderer_cleanup();
        stats_cleanup();
        return EXIT_FAILURE;
    }

    int recv_idx = 0;

    frame_state_s state;
    frame_state_init(&state, cfg->image, (uint16_t)bpp);

    GLFWwindow * window = renderer_get_window();
    mouse_state_init(window);

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

        // Progress bar — клик и скраб.
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

        bool step_this_iter = false;
        if (g_step_pending != 0)
        {
            if (g_step_pending > 0)
            {
                source_step_forward(g_source);
            }
            else
            {
                source_step_backward(g_source);
            }
            g_step_pending = 0;
            step_this_iter = true;
        }
        if (g_seek_pending)
        {
            g_seek_pending = false;
            step_this_iter = true;
        }

        const bool skip_receive = g_paused && cfg->use_file_source && !step_this_iter;

        int received = 0;
        if (!skip_receive)
        {
            received = source_receive(src, &state, g_frame_buffers[ recv_idx ]);
            if (received < 0)
            {
                log_error("app: source receive failed\n");
                break;
            }
            if (received > 0 && !cfg->use_file_source)
            {
                stats_add_packets(received);
            }
        }

        bool frame_ready = cfg->use_file_source ? (received == 1)
                                                : (frame_state_is_ready(&state) || source_check_timeout(src, &state));

        if (frame_ready)
        {
            if (g_paused && cfg->use_file_source)
            {
                memcpy(g_paused_buffer, g_frame_buffers[ recv_idx ], g_frame_bytes);
            }

            renderer_update_texture(g_paused ? g_paused_buffer : g_frame_buffers[ recv_idx ]);

            saver_add_frame(g_frame_buffers[ recv_idx ]);
            stats_frame_rendered();

            g_last_rendered_idx     = recv_idx;
            g_has_rendered          = true;
            g_last_paused_render_us = get_time_us();

            int cur = 0;
            int tot = 0;
            if (source_get_frame_position(src, &cur, &tot))
            {
                window_title_set_frame(cur, tot);
                progress_bar_set(cur, tot);
            }

            if (step_this_iter)
            {
                window_title_refresh();
            }

            recv_idx = (recv_idx + 1) % NUM_FRAME_BUFFERS;
            frame_state_reset(&state);
        }
        else if (g_paused && g_has_rendered && g_paused_buffer)
        {
            uint64_t now_us = get_time_us();
            if (now_us - g_last_paused_render_us >= 100000)
            {
                renderer_update_texture(g_paused_buffer);
                g_last_paused_render_us = now_us;
            }
        }

        stats_update();
        SLEEP_US(200);
    }

    g_source = nullptr;
    cleanup_buffers();

    saver_cleanup();
    stats_cleanup();
    renderer_cleanup();
    source_close(src);

#ifdef _WIN32
    timeEndPeriod(1);
#endif
    return EXIT_SUCCESS;
}
