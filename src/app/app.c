#include "app.h"
#include "pipeline.h"

#include "config.h"
#include "control/hotkeys.h"
#include "control/mouse.h"
#include "log/logger.h"
#include "log/stats.h"
#include "render/renderer.h"
#include "render/ui/progress_bar.h"
#include "render/ui/svg_icons.h"
#include "render/ui/window_title.h"
#include "render/window.h"
#include "save/saver.h"
#include "save/screenshot.h"
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
#endif

//------------------------------------------------------- Состояние ----------------------------------------------------

static volatile bool g_running    = true;
static const app_config_s * g_cfg = nullptr;
static source_s * g_source        = nullptr;

static uint8_t * g_main_buffer = nullptr;
static size_t g_frame_bytes    = 0;

static bool g_scrubbing = false;

static void signal_handler(int /* unused */)
{
    g_running = false;
}

//------------------------------------------------------- Управление --------------------------------------------------

bool app_toggle_pause(void)
{
    bool paused = pipeline_toggle_pause();
    window_title_set_paused(paused);
    return paused;
}

bool app_is_paused(void)
{
    return pipeline_is_paused();
}

bool app_step_forward(void)
{
    if (!g_cfg || !g_cfg->use_file_source)
    {
        return false;
    }
    pipeline_step_forward();
    return true;
}

bool app_step_backward(void)
{
    if (!g_cfg || !g_cfg->use_file_source)
    {
        return false;
    }
    pipeline_step_backward();
    return true;
}

bool app_seek_to(int target_frame)
{
    return pipeline_seek_to(target_frame);
}

bool app_seek_fraction(float fraction)
{
    return pipeline_seek_fraction(fraction);
}

//------------------------------------------------------- Инициализация ------------------------------------------------

static bool app_init(const app_config_s * cfg)
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
        log_error("app: Failed to initialize OpenGL");
        goto fail_renderer;
    }

    if (!svg_icons_init())
    {
        // Non-fatal: fallback to primitive icons
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

    g_source = source_create(&src_params);
    if (!source_open(g_source, &src_params))
    {
        log_error("app: Failed to open data source");
        goto fail_source;
    }

    signal(SIGINT, signal_handler);

    g_frame_bytes = cfg->image.x * cfg->image.y * bpp;
    g_main_buffer = (uint8_t *)calloc(1, g_frame_bytes);
    if (!g_main_buffer)
    {
        log_error("app: Failed to allocate main buffer");
        goto fail_buffer;
    }

    if (!pipeline_start(g_source, cfg->image, bpp, g_frame_bytes, cfg->use_file_source))
    {
        log_error("app: Failed to start pipeline");
        goto fail_pipeline;
    }

    int cur = 0;
    int tot = 0;
    if (pipeline_get_position(&cur, &tot))
    {
        window_title_set_frame(cur, tot);
        progress_bar_set(cur, tot);
    }

    GLFWwindow * window = window_get();
    mouse_state_init(window);
    hotkeys_init(window);

    return true;

    // --- Cleanup on failure (reverse order of init) ---

fail_pipeline:
    free(g_main_buffer);
    g_main_buffer = nullptr;
fail_buffer:
    source_close(g_source);
    g_source = nullptr;
fail_source:
    saver_cleanup();
    svg_icons_cleanup();
    renderer_cleanup();
fail_renderer:
    stats_cleanup();
#ifdef _WIN32
    timeEndPeriod(1);
#endif
    return false;
}

//------------------------------------------------------- Ввод / UI ----------------------------------------------------

static void app_process_input(GLFWwindow * window, mouse_state_s * mouse)
{
    mouse_state_update(window, mouse);

    double wheel        = mouse_state_take_wheel(mouse);
    point_s mouse_point = {mouse->x, mouse->y};

    if (wheel != 0.0)
    {
        renderer_zoom_apply_wheel(wheel, mouse_point);
    }
    renderer_pan_apply_drag(mouse_point, mouse->middle);

    bool menu_consumed = renderer_handle_context_menu(mouse_point, mouse->right, mouse->left);

    // Progress bar
    bool progress_consumed = false;
    if (!menu_consumed && g_cfg->use_file_source)
    {
        float frac  = 0.0f;
        bool on_bar = progress_bar_hit_test(mouse_point, renderer_get_dpi_scale(), &frac);
        if (mouse->left && (on_bar || g_scrubbing))
        {
            app_seek_fraction(frac);
            g_scrubbing       = true;
            progress_consumed = true;
        }
        else if (!mouse->left)
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
        renderer_set_selection_from_mouse(frame, mouse->left, in_view);
    }
}

//------------------------------------------------------- Обновление пайплайна -----------------------------------------

static void app_update_pipeline_and_ui(void)
{
    // Забираем новый кадр из pipeline
    if (pipeline_take_frame(g_main_buffer, g_frame_bytes))
    {
        renderer_set_frame(g_main_buffer);
    }

    // Обновляем позицию в заголовке и прогрессбаре
    int cur = 0;
    int tot = 0;
    if (pipeline_get_position(&cur, &tot) && tot > 0)
    {
        window_title_set_frame(cur, tot);
        progress_bar_set(cur, tot);
    }
}

//------------------------------------------------------- Очистка -----------------------------------------------------

static void app_cleanup(void)
{
    pipeline_stop();
    free(g_main_buffer);
    g_main_buffer = nullptr;

    saver_cleanup();
    stats_cleanup();
    svg_icons_cleanup();
    renderer_cleanup();
    source_close(g_source);
    g_source = nullptr;

#ifdef _WIN32
    timeEndPeriod(1);
#endif
}

//------------------------------------------------------- Main ---------------------------------------------------------

int app_run(const app_config_s * cfg)
{
    if (!app_init(cfg))
    {
        return EXIT_FAILURE;
    }

    GLFWwindow * window = window_get();
    mouse_state_s mouse;

    while (g_running && !glfwWindowShouldClose(window))
    {
        window_poll_events();

        app_process_input(window, &mouse);
        app_update_pipeline_and_ui();

        renderer_draw();
        stats_update();
    }

    app_cleanup();
    return EXIT_SUCCESS;
}
