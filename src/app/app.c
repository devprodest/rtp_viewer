#include "app.h"
#include "config.h"


#include "control/hotkeys.h"
#include "control/mouse.h"
#include "log/stats.h"
#include "render/renderer.h"
#include "save/saver.h"
#include "source/source_file.h"
#include "source/source_rtp.h"
#include "util/version.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#define NUM_FRAME_BUFFERS 2

static volatile bool g_running = true;

static uint8_t * g_frame_buffers[NUM_FRAME_BUFFERS] = {nullptr, nullptr};
static uint8_t * g_paused_buffer                    = nullptr;
static int       g_last_rendered_idx                = 0;
static bool      g_has_rendered                     = false;
static size_t    g_frame_bytes                      = 0;
static bool      g_paused                           = false;

static const app_config_s * g_cfg = nullptr;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
}

bool app_toggle_pause(void)
{
    g_paused = !g_paused;
    if (g_paused && g_has_rendered && g_paused_buffer)
    {
        memcpy(g_paused_buffer, g_frame_buffers[g_last_rendered_idx], g_frame_bytes);
    }
    renderer_set_paused(g_paused);
    return g_paused;
}

static void cleanup_buffers(void)
{
    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        free(g_frame_buffers[i]);
        g_frame_buffers[i] = nullptr;
    }
    free(g_paused_buffer);
    g_paused_buffer = nullptr;
}

int app_run(const app_config_s * cfg)
{
    g_cfg = cfg;

    fprintf(stderr, "%s v%s\n", RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR);

    stats_init(cfg->logfile);

    if (!renderer_init(cfg->width, cfg->height, cfg->format, cfg->effective_bits, cfg->smooth_start))
    {
        fprintf(stderr, "Error: Не удалось инициализировать OpenGL\n");
        stats_cleanup();
        return EXIT_FAILURE;
    }

    int bpp = renderer_get_bytes_per_pixel();

    saver_init(cfg->save_prefix, cfg->outdir, cfg->batch_size, cfg->time_window, cfg->width, cfg->height, bpp,
               cfg->format);

    renderer_refresh_title();

    int  sock = -1;

    if (cfg->use_file_source)
    {
        if (!file_source_open(cfg->source_file, cfg->width, cfg->height, bpp, cfg->mock_fps))
        {
            fprintf(stderr, "Error: Не удалось открыть источник %s\n", cfg->source_file);
            saver_cleanup();
            renderer_cleanup();
            stats_cleanup();
            return EXIT_FAILURE;
        }
    }
    else
    {
        sock = rtp_socket_open(cfg->port);
        if (sock < 0)
        {
            fprintf(stderr, "Error: Не удалось открыть UDP сокет на порту %d\n", cfg->port);
            renderer_cleanup();
            stats_cleanup();
            return EXIT_FAILURE;
        }
    }

    signal(SIGINT, signal_handler);

    g_frame_bytes = (size_t)cfg->width * (size_t)cfg->height * (size_t)bpp;

    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        g_frame_buffers[i] = calloc(1, g_frame_bytes);
        if (!g_frame_buffers[i])
        {
            perror("calloc frame buffer");
            cleanup_buffers();
            renderer_cleanup();
            if (sock >= 0) RTP_CLOSE_SOCKET(sock);
            stats_cleanup();
            return EXIT_FAILURE;
        }
    }

    g_paused_buffer = calloc(1, g_frame_bytes);
    if (!g_paused_buffer)
    {
        perror("calloc paused buffer");
        cleanup_buffers();
        renderer_cleanup();
        if (sock >= 0) RTP_CLOSE_SOCKET(sock);
        stats_cleanup();
        return EXIT_FAILURE;
    }

    int recv_idx = 0;

    rtp_frame_state_s state;
    rtp_frame_state_init(&state, (uint16_t)cfg->width, (uint16_t)cfg->height, (uint16_t)bpp);

    GLFWwindow * window = renderer_get_window();
    mouse_state_init(window);

    mouse_state_s mouse;

    while (g_running && !glfwWindowShouldClose(window))
    {
        renderer_poll_events();
        mouse_state_update(window, &mouse);
        hotkeys_process(window);

        int  received;
        bool frame_ready;

        if (cfg->use_file_source)
        {
            received = file_source_receive_frame(&state, g_frame_buffers[recv_idx]);
            if (received < 0)
            {
                fprintf(stderr, "Error: file_source read error\n");
                break;
            }
            state.frame_ready = (received == 1);
            frame_ready       = state.frame_ready;
        }
        else
        {
            received = rtp_receive_packets(sock, &state, g_frame_buffers[recv_idx]);
            if (received > 0)
            {
                stats_add_packets(received);
            }
            frame_ready = rtp_frame_is_ready(&state) || rtp_frame_check_timeout(&state);
        }

        if (frame_ready)
        {
            renderer_update_texture(g_paused ? g_paused_buffer : g_frame_buffers[recv_idx]);

            saver_add_frame(g_frame_buffers[recv_idx]);
            stats_frame_rendered();

            g_last_rendered_idx = recv_idx;
            g_has_rendered      = true;

            recv_idx = (recv_idx + 1) % NUM_FRAME_BUFFERS;
            rtp_frame_state_reset(&state);
        }

        stats_update();
        SLEEP_MS(1);
    }

    cleanup_buffers();

    saver_cleanup();
    stats_cleanup();
    renderer_cleanup();
    file_source_close();
    if (sock >= 0)
    {
        RTP_CLOSE_SOCKET(sock);
    }

    return EXIT_SUCCESS;
}