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

#include "gl_render.h"
#include "rtp.h"
#include "saver.h"
#include "stats.h"
#include "utils.h"

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

static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
}

static void print_usage(const char * prog)
{
    printf(
        "Usage: %s -x <width> -y <height> -p <port> -f <format> [options]\n"
        "\n"
        "Required:\n"
        "  -x <width>          Video width in pixels (1..65535)\n"
        "  -y <height>         Video height in pixels (1..65535)\n"
        "  -p <port>           UDP port to listen on (1..65535)\n"
        "  -f <format>         Pixel format name (e.g. gray8, rgb888, bayerRGGB)\n"
        "                      File formats/<format>.fmt must exist\n"
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
        "\n"
        "Hotkeys:\n"
        "  S          Toggle smoothing (linear/nearest)\n"
        "  R          Toggle recording (on/off)\n"
        "  W          Force save current accumulated frames\n"
        "  Esc        Exit\n",
        prog, DEFAULT_BATCH_SIZE);
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
    static bool s_pressed = false;
    static bool r_pressed = false;
    static bool w_pressed = false;

    bool s   = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool r   = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    bool w   = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool esc = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;

    if (s && !s_pressed)
    {
        gl_render_toggle_smoothing();
    }

    if (r && !r_pressed)
    {
        saver_toggle_recording();
        gl_render_refresh_title();
    }

    if (w && !w_pressed)
    {
        saver_flush();
    }

    if (esc)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    s_pressed = s;
    r_pressed = r;
    w_pressed = w;
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
        {"help",  no_argument, nullptr, 'h'},
        {nullptr, 0,           nullptr, 0  }
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "x:y:p:f:e:s:b:t:l:o:Sh", long_opts, nullptr)) != -1)
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

            case 'S':
                smooth_start = true;
                break;

            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;

            case '?':
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Error: неизвестный аргумент: '%s'\n", argv[optind]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (width == 0 || height == 0 || port == 0 || format == nullptr)
    {
        fprintf(stderr, "Error: -x, -y, -p и -f обязательные аргументы\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    {
        char fmt_path[512];
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
            char dirbuf[512];
            size_t n = (size_t)(slash - logfile);
            if (n >= sizeof(dirbuf))
            {
                fprintf(stderr, "Error: Слишком длинный пусть к файлу лога\n");
                return EXIT_FAILURE;
            }
            memcpy(dirbuf, logfile, n);
            dirbuf[n] = '\0';
            if (!dir_exists(dirbuf))
            {
                fprintf(stderr, "Error: Директория для сохранения лога не сужществует: %s\n", dirbuf);
                return EXIT_FAILURE;
            }
        }
    }

    stats_init(logfile);

    if (!gl_render_init(width, height, format, effective_bits, smooth_start))
    {
        fprintf(stderr, "Error: Не удалось инициализировать OpenGL\n");
        stats_cleanup();
        return EXIT_FAILURE;
    }

    int bpp = gl_render_get_bytes_per_pixel();

    saver_init(save_prefix, outdir, batch_size, time_window, width, height, bpp, format);

    // Заголовок окна содержит актуальное состояние записи
    gl_render_refresh_title();

    int sock = rtp_socket_open(port);
    if (sock < 0)
    {
        fprintf(stderr, "Error: Не удалось открыть UDP сокет на порту %d\n", port);
        gl_render_cleanup();
        stats_cleanup();
        return EXIT_FAILURE;
    }

    signal(SIGINT, signal_handler);

    uint8_t * frame_buffers[NUM_FRAME_BUFFERS];
    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        frame_buffers[i] = calloc(1, (size_t)width * height * (size_t)bpp);
        if (!frame_buffers[i])
        {
            perror("calloc frame buffer");
            for (int j = 0; j < i; j++)
            {
                free(frame_buffers[j]);
            }
            gl_render_cleanup();
            RTP_CLOSE_SOCKET(sock);
            stats_cleanup();
            return EXIT_FAILURE;
        }
    }

    int recv_idx = 0;

    rtp_frame_state_s state;
    rtp_frame_state_init(&state, (uint16_t)width, (uint16_t)height, (uint16_t)bpp);

    GLFWwindow * window = gl_render_get_window();

    while (g_running && !glfwWindowShouldClose(window))
    {
        gl_render_poll_events();
        process_input(window);

        int received = rtp_receive_packets(sock, &state, frame_buffers[recv_idx]);
        if (received > 0)
        {
            stats_add_packets(received);
        }

        bool frame_ready = rtp_frame_is_ready(&state) || rtp_frame_check_timeout(&state);

        if (frame_ready)
        {
            gl_render_update_texture(frame_buffers[recv_idx]);
            saver_add_frame(frame_buffers[recv_idx]);
            stats_frame_rendered();

            recv_idx = (recv_idx + 1) % NUM_FRAME_BUFFERS;
            rtp_frame_state_reset(&state);
        }

        stats_update();
        SLEEP_MS(1);
    }

    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        free(frame_buffers[i]);
    }

    saver_cleanup();
    stats_cleanup();
    gl_render_cleanup();
    RTP_CLOSE_SOCKET(sock);

    return EXIT_SUCCESS;
}
