// rtp_viewer - приём несжатого RTP видео (RFC 4175), отображение через OpenGL

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "control/mouse.h"
#include "help/help_dialog.h"
#include "log/stats.h"
#include "render/renderer.h"
#include "save/saver.h"
#include "source/source_file.h"
#include "source/source_rtp.h"
#include "util/utils.h"
#include "util/version.h"

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

#define NUM_FRAME_BUFFERS  2
#define DEFAULT_BATCH_SIZE 500

static volatile bool g_running = true;

static uint8_t * g_frame_buffers[ NUM_FRAME_BUFFERS ] = {nullptr, nullptr};

static bool g_paused             = false;
static uint8_t * g_paused_buffer = nullptr;
static int g_last_rendered_idx   = 0;
static bool g_has_rendered       = false;
static size_t g_frame_bytes      = 0;

const char * source_file = nullptr;
int mock_fps             = 25;
bool seen_i              = false;

static const uint8_t * frame_buffers_for_pause(void)
{
    return g_frame_buffers[ g_last_rendered_idx ];
}

static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
}

static void print_usage(const char * prog)
{
    printf(
        "%s v%s\n"
        "\n"
        "Usage: %s -x <width> -y <height> [-p <port> | -i <file>] -f <format> [options]\n"
        "\n"
        "Required:\n"
        "  -x <width>          Video width in pixels (1..65535)\n"
        "  -y <height>         Video height in pixels (1..65535)\n"
        "  -f <format>         Pixel format name (e.g. gray8, rgb888, bayerRGGB)\n"
        "                      File formats/<format>.fmt must exist\n"
        "\n"
        "Input (one of):\n"
        "  -p <port>           UDP port to listen on (1..65535)\n"
        "  -i <file>           Replay .raw file instead of UDP\n"
        "                      Frames are read at mock speed and looped\n"
        "\n"
        "Optional:\n"
        "  -e <bits>           Effective bits for gray16 (1..16, default 16)\n"
        "  -s <prefix>         Save raw frames from start (prefix for file names).\n"
        "                      If not given, files still can be recorded with 'R'\n"
        "                      using default prefix 'rtp'.\n"
        "  -b <N>              Frames per saved file (>=1, default %d)\n"
        "  -t <seconds>        Save interval in seconds (>=0, default 0 = off)\n"
        "  -l <logfile>        Log statistics to file\n"
        "  -o <dir>            Output directory for saved files (default \".\")\n"
        "  -S                  Start with smoothing enabled (default off)\n"
        "  -h, --help          Show this help\n"
        "  -V, --version       Show program version\n"
        "\n"
        "Hotkeys:\n"
        "  1          Toggle smoothing (linear/nearest)\n"
        "  E          Toggle saturation markers (blue=under, red=over)\n"
        "  S          Save screenshot (PNG)\n"
        "  R          Toggle recording (on/off)\n"
        "  W          Force save current accumulated frames\n"
        "  H          Toggle histogram overlay\n"
        "  Ctrl+H     Toggle histogram scale (linear/log)\n"
        "  C          Toggle histogram CDF overlay\n"
        "  Space      Pause / resume display and analysis\n"
        "  F1         Show this help\n",
        RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR, prog, DEFAULT_BATCH_SIZE);
}

static bool file_exists(const char * path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool dir_exists(const char * path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void process_input(GLFWwindow * window)
{
    static bool s_pressed     = false;
    static bool r_pressed     = false;
    static bool w_pressed     = false;
    static bool one_pressed   = false;
    static bool f1_pressed    = false;
    static bool h_pressed     = false;
    static bool h_was_down    = false; // для edge-trigger на Ctrl+H
    static bool space_pressed = false;
    static bool e_pressed     = false;
    static bool c_pressed     = false;

    bool c     = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    bool s     = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool r     = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    bool w     = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool one   = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool f1    = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    bool h     = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool e     = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    bool ctrl  = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                 glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;


    if (e && !e_pressed)
    {
        renderer_toggle_saturation();
    }

    if (one && !one_pressed)
    {
        renderer_toggle_smoothing();
    }

    if (s && !s_pressed)
    {
        renderer_request_screenshot();
    }

    if (r && !r_pressed)
    {
        saver_toggle_recording();
        renderer_refresh_title();
    }

    if (w && !w_pressed)
    {
        saver_flush();
    }

    if (f1 && !f1_pressed)
    {
        help_dialog_show();
    }

    if (h && ctrl && !h_was_down)
    {
        renderer_toggle_histogram_scale();
    }
    else if (h && !ctrl && !h_pressed)
    {
        renderer_toggle_histogram();
    }

    if (space && !space_pressed)
    {
        g_paused = !g_paused;
        if (g_paused && g_has_rendered && g_paused_buffer)
        {
            memcpy(g_paused_buffer, frame_buffers_for_pause(), g_frame_bytes);
        }
        renderer_set_paused(g_paused);
    }

    if (c && !c_pressed)
    {
        renderer_toggle_histogram_cdf();
    }

    c_pressed     = c;
    e_pressed     = e;
    s_pressed     = s;
    r_pressed     = r;
    w_pressed     = w;
    one_pressed   = one;
    f1_pressed    = f1;
    h_pressed     = h && !ctrl;
    h_was_down    = h;
    space_pressed = space;
}

int main(int argc, char ** argv)
{
    int width           = 0;
    int height          = 0;
    int port            = 0;
    const char * format = nullptr;

    const char * save_prefix = nullptr;
    const char * logfile     = nullptr;
    const char * outdir      = ".";
    int effective_bits       = 16;
    int batch_size           = DEFAULT_BATCH_SIZE;
    int time_window          = 0;
    bool smooth_start        = false;

    bool seen_x = false;
    bool seen_y = false;
    bool seen_p = false;
    bool seen_f = false;
    bool seen_e = false;
    bool seen_s = false;
    bool seen_b = false;
    bool seen_t = false;
    bool seen_l = false;
    bool seen_o = false;

    static struct option long_opts[] = {
        {"help",    no_argument,       nullptr, 'h'},
        {"version", no_argument,       nullptr, 'V'},
        {"input",   required_argument, nullptr, 'i'},
        {nullptr,   0,                 nullptr, 0  }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "x:y:p:f:e:s:b:t:l:o:i:ShV", long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'x':
            {
                if (seen_x)
                {
                    fprintf(stderr, "Error: -x определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                long v = 0;
                if (parse_int(optarg, 1, 65535, &v) != 0)
                {
                    fprintf(stderr, "Error: -x должен быть в диапазоне [1..65535], сейчас '%s'\n", optarg);
                    return EXIT_FAILURE;
                }
                width  = (int)v;
                seen_x = true;
            }
            break;

            case 'y':
            {
                if (seen_y)
                {
                    fprintf(stderr, "Error: -y определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                long v = 0;
                if (parse_int(optarg, 1, 65535, &v) != 0)
                {
                    fprintf(stderr, "Error: -y должен быть в диапазоне [1..65535], сейчас '%s'\n", optarg);
                    return EXIT_FAILURE;
                }
                height = (int)v;
                seen_y = true;
            }
            break;

            case 'p':
            {
                if (seen_p)
                {
                    fprintf(stderr, "Error: -p определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                long v = 0;
                if (parse_int(optarg, 1, 65535, &v) != 0)
                {
                    fprintf(stderr, "Error: -p UDP порт [1..65535], сейчас '%s'\n", optarg);
                    return EXIT_FAILURE;
                }
                port   = (int)v;
                seen_p = true;
            }
            break;

            case 'i':
            {
                if (seen_i)
                {
                    fprintf(stderr, "Error: -i определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -i ожидается путь к .raw-файлу\n");
                    return EXIT_FAILURE;
                }
                source_file = optarg;
                seen_i      = true;
            }
            break;

            case 'f':
            {
                if (seen_f)
                {
                    fprintf(stderr, "Error: -f определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -f должен быть именем формата\n");
                    return EXIT_FAILURE;
                }
                format = optarg;
                seen_f = true;
            }
            break;

            case 'e':
            {
                if (seen_e)
                {
                    fprintf(stderr, "Error: -e определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                long v = 0;
                if (parse_int(optarg, 1, 16, &v) != 0)
                {
                    fprintf(stderr, "Error: -e должен быть в диапазоне [1..16], сейчас '%s'\n", optarg);
                    return EXIT_FAILURE;
                }
                effective_bits = (int)v;
                seen_e         = true;
            }
            break;

            case 's':
            {
                if (seen_s)
                {
                    fprintf(stderr, "Error: -s определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -s Ожидается префикс имени\n");
                    return EXIT_FAILURE;
                }
                save_prefix = optarg;
                seen_s      = true;
            }
            break;

            case 'b':
            {
                if (seen_b)
                {
                    fprintf(stderr, "Error: -b определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                long v = 0;
                if (parse_int(optarg, 1, INT_MAX, &v) != 0)
                {
                    fprintf(stderr, "Error: -b ожидается число >= 1, сейчас '%s'\n", optarg);
                    return EXIT_FAILURE;
                }
                batch_size = (int)v;
                seen_b     = true;
            }
            break;

            case 't':
            {
                if (seen_t)
                {
                    fprintf(stderr, "Error: -t определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                long v = 0;
                if (parse_int(optarg, 0, INT_MAX, &v) != 0)
                {
                    fprintf(stderr, "Error: -t  ожидается число >= 0, сейчас '%s'\n", optarg);
                    return EXIT_FAILURE;
                }
                time_window = (int)v;
                seen_t      = true;
            }
            break;

            case 'l':
            {
                if (seen_l)
                {
                    fprintf(stderr, "Error: -l определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -l ожидается имя файла\n");
                    return EXIT_FAILURE;
                }
                logfile = optarg;
                seen_l  = true;
            }
            break;

            case 'o':
            {
                if (seen_o)
                {
                    fprintf(stderr, "Error: -o определен более одного раза\n");
                    return EXIT_FAILURE;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -o ожидается пусть к директории для сохранения\n");
                    return EXIT_FAILURE;
                }
                outdir = optarg;
                seen_o = true;
            }
            break;

            case 'S': smooth_start = true; break;

            case 'h': print_usage(argv[ 0 ]); return EXIT_SUCCESS;

            case 'V':
                printf("%s v%s\n%s\n", RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR, RTP_VIEWER_COPYRIGHT);
                return EXIT_SUCCESS;

            case '?':
            default: print_usage(argv[ 0 ]); return EXIT_FAILURE;
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Error: неизвестный аргумент: '%s'\n", argv[ optind ]);
        print_usage(argv[ 0 ]);
        return EXIT_FAILURE;
    }

    if (width == 0 || height == 0 || format == nullptr)
    {
        fprintf(stderr, "Error: -x, -y и -f обязательные аргументы\n");
        print_usage(argv[ 0 ]);
        return EXIT_FAILURE;
    }

    if (!source_file && port == 0)
    {
        fprintf(stderr, "Error: -p обязателен, если не задан -i\n");
        print_usage(argv[ 0 ]);
        return EXIT_FAILURE;
    }

    {
        char fmt_path[ 512 ];
        snprintf(fmt_path, sizeof(fmt_path), "formats/%s.fmt", format);
        if (!file_exists(fmt_path))
        {
            fprintf(stderr, "Error: Не найден файл описания формата: %s\n", fmt_path);
            return EXIT_FAILURE;
        }
    }

    // Директорию сохранения проверяем только если будем писать автоматически.
    // При записи по клавише 'R' файлы будут падать в текущую директорию,
    // если -o не задан - это допустимо.
    if (save_prefix && !dir_exists(outdir))
    {
        fprintf(stderr, "Error: Директория для сохранения файлов не сужществует: %s\n", outdir);
        return EXIT_FAILURE;
    }

    if (logfile)
    {
        const char * slash = strrchr(logfile, '/');
#ifdef _WIN32
        const char * bslash = strrchr(logfile, '\\');
        if (bslash && (!slash || bslash > slash))
        {
            slash = bslash;
        }
#endif

        if (slash && slash != logfile)
        {
            char dirbuf[ 512 ];
            size_t n = (size_t)(slash - logfile);
            if (n >= sizeof(dirbuf))
            {
                fprintf(stderr, "Error: Слишком длинный пусть к файлу лога\n");
                return EXIT_FAILURE;
            }
            memcpy(dirbuf, logfile, n);
            dirbuf[ n ] = '\0';
            if (!dir_exists(dirbuf))
            {
                fprintf(stderr, "Error: Директория для сохранения лога не сужществует: %s\n", dirbuf);
                return EXIT_FAILURE;
            }
        }
    }

    fprintf(stderr, "%s v%s\n", RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR);

    stats_init(logfile);

    if (!renderer_init(width, height, format, effective_bits, smooth_start))
    {
        fprintf(stderr, "Error: Не удалось инициализировать OpenGL\n");
        stats_cleanup();
        return EXIT_FAILURE;
    }

    int bpp = renderer_get_bytes_per_pixel();

    saver_init(save_prefix, outdir, batch_size, time_window, width, height, bpp, format);

    // Заголовок окна содержит актуальное состояние записи
    renderer_refresh_title();

    int sock             = -1;
    bool use_file_source = (source_file != nullptr);

    if (use_file_source)
    {
        if (!file_source_open(source_file, width, height, renderer_get_bytes_per_pixel(), mock_fps))
        {
            fprintf(stderr, "Error: Не удалось открыть источник %s\n", source_file);
            saver_cleanup();
            renderer_cleanup();
            stats_cleanup();
            return EXIT_FAILURE;
        }
    }
    else
    {
        sock = rtp_socket_open(port);
        if (sock < 0)
        {
            fprintf(stderr, "Error: Не удалось открыть UDP сокет на порту %d\n", port);
            renderer_cleanup();
            stats_cleanup();
            return EXIT_FAILURE;
        }
    }

    signal(SIGINT, signal_handler);

    g_frame_bytes = (size_t)width * (size_t)height * (size_t)bpp;

    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        g_frame_buffers[ i ] = calloc(1, g_frame_bytes);
        if (!g_frame_buffers[ i ])
        {
            perror("calloc frame buffer");
            for (int j = 0; j < i; j++)
            {
                free(g_frame_buffers[ j ]);
            }
            renderer_cleanup();
            RTP_CLOSE_SOCKET(sock);
            stats_cleanup();
            return EXIT_FAILURE;
        }
    }

    // Буфер для pause: третий, не участвует в обороте RTP.
    g_paused_buffer = calloc(1, g_frame_bytes);
    if (!g_paused_buffer)
    {
        perror("calloc paused buffer");
        for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
        {
            free(g_frame_buffers[ i ]);
        }
        renderer_cleanup();
        RTP_CLOSE_SOCKET(sock);
        stats_cleanup();
        return EXIT_FAILURE;
    }


    int recv_idx = 0;

    rtp_frame_state_s state;
    rtp_frame_state_init(&state, (uint16_t)width, (uint16_t)height, (uint16_t)bpp);

    GLFWwindow * window = renderer_get_window();
    mouse_state_init(window);

    mouse_state_s mouse;

    while (g_running && !glfwWindowShouldClose(window))
    {
        renderer_poll_events();
        mouse_state_update(window, &mouse);
        process_input(window);

        int received;
        bool frame_ready;

        if (use_file_source)
        {
            received = file_source_receive_frame(&state, g_frame_buffers[ recv_idx ]);
            if (received < 0)
            {
                fprintf(stderr, "Error: file_source read error\n");
                break;
            }
            // Кадр готов ровно тогда, когда receive вернул 1.
            state.frame_ready = (received == 1);
            frame_ready       = state.frame_ready;
        }
        else
        {
            received = rtp_receive_packets(sock, &state, g_frame_buffers[ recv_idx ]);
            if (received > 0)
            {
                stats_add_packets(received);
            }
            frame_ready = rtp_frame_is_ready(&state) || rtp_frame_check_timeout(&state);
        }

        if (frame_ready)
        {
            if (g_paused)
            {
                
                // Показываем снапшот. Saver продолжает писать живые кадры.
                renderer_update_texture(g_paused_buffer);
            }
            else
            {
                renderer_update_texture(g_frame_buffers[ recv_idx ]);
            }

            saver_add_frame(g_frame_buffers[ recv_idx ]);
            stats_frame_rendered();

            g_last_rendered_idx = recv_idx;
            g_has_rendered      = true;

            recv_idx = (recv_idx + 1) % NUM_FRAME_BUFFERS;
            rtp_frame_state_reset(&state);
        }

        stats_update();
        SLEEP_MS(1);
    }

    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        free(g_frame_buffers[ i ]);
    }

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
