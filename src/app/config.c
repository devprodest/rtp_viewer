#include "config.h"

#include "util/utils.h"
#include "util/version.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define DEFAULT_BATCH_SIZE 500

void app_config_print_usage(const char * prog)
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

bool app_config_parse(int argc, char ** argv, app_config_s * cfg, bool * exit_ok)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->outdir         = ".";
    cfg->effective_bits = 16;
    cfg->batch_size     = DEFAULT_BATCH_SIZE;
    cfg->mock_fps       = 25;
    *exit_ok            = false;

    bool seen_x = false, seen_y = false, seen_p = false, seen_f = false;
    bool seen_e = false, seen_s = false, seen_b = false, seen_t = false;
    bool seen_l = false, seen_o = false, seen_i = false;

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
                if (seen_x)
                {
                    fprintf(stderr, "Error: -x определен более одного раза\n");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, 65535, &v) != 0)
                    {
                        fprintf(stderr, "Error: -x должен быть в диапазоне [1..65535], сейчас '%s'\n", optarg);
                        return false;
                    }
                    cfg->width = (int)v;
                }
                seen_x = true;
                break;

            case 'y':
                if (seen_y)
                {
                    fprintf(stderr, "Error: -y определен более одного раза\n");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, 65535, &v) != 0)
                    {
                        fprintf(stderr, "Error: -y должен быть в диапазоне [1..65535], сейчас '%s'\n", optarg);
                        return false;
                    }
                    cfg->height = (int)v;
                }
                seen_y = true;
                break;

            case 'p':
                if (seen_p)
                {
                    fprintf(stderr, "Error: -p определен более одного раза\n");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, 65535, &v) != 0)
                    {
                        fprintf(stderr, "Error: -p UDP порт [1..65535], сейчас '%s'\n", optarg);
                        return false;
                    }
                    cfg->port = (int)v;
                }
                seen_p = true;
                break;

            case 'i':
                if (seen_i)
                {
                    fprintf(stderr, "Error: -i определен более одного раза\n");
                    return false;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -i ожидается путь к .raw-файлу\n");
                    return false;
                }
                cfg->source_file = optarg;
                seen_i           = true;
                break;

            case 'f':
                if (seen_f)
                {
                    fprintf(stderr, "Error: -f определен более одного раза\n");
                    return false;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -f должен быть именем формата\n");
                    return false;
                }
                cfg->format = optarg;
                seen_f      = true;
                break;

            case 'e':
                if (seen_e)
                {
                    fprintf(stderr, "Error: -e определен более одного раза\n");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, 16, &v) != 0)
                    {
                        fprintf(stderr, "Error: -e должен быть в диапазоне [1..16], сейчас '%s'\n", optarg);
                        return false;
                    }
                    cfg->effective_bits = (int)v;
                }
                seen_e = true;
                break;

            case 's':
                if (seen_s)
                {
                    fprintf(stderr, "Error: -s определен более одного раза\n");
                    return false;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -s Ожидается префикс имени\n");
                    return false;
                }
                cfg->save_prefix = optarg;
                seen_s           = true;
                break;

            case 'b':
                if (seen_b)
                {
                    fprintf(stderr, "Error: -b определен более одного раза\n");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, INT_MAX, &v) != 0)
                    {
                        fprintf(stderr, "Error: -b ожидается число >= 1, сейчас '%s'\n", optarg);
                        return false;
                    }
                    cfg->batch_size = (int)v;
                }
                seen_b = true;
                break;

            case 't':
                if (seen_t)
                {
                    fprintf(stderr, "Error: -t определен более одного раза\n");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 0, INT_MAX, &v) != 0)
                    {
                        fprintf(stderr, "Error: -t  ожидается число >= 0, сейчас '%s'\n", optarg);
                        return false;
                    }
                    cfg->time_window = (int)v;
                }
                seen_t = true;
                break;

            case 'l':
                if (seen_l)
                {
                    fprintf(stderr, "Error: -l определен более одного раза\n");
                    return false;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -l ожидается имя файла\n");
                    return false;
                }
                cfg->logfile = optarg;
                seen_l       = true;
                break;

            case 'o':
                if (seen_o)
                {
                    fprintf(stderr, "Error: -o определен более одного раза\n");
                    return false;
                }
                if (*optarg == '\0')
                {
                    fprintf(stderr, "Error: -o ожидается путь к директории для сохранения\n");
                    return false;
                }
                cfg->outdir = optarg;
                seen_o      = true;
                break;

            case 'S': cfg->smooth_start = true; break;

            case 'h':
                app_config_print_usage(argv[ 0 ]);
                *exit_ok = true;
                return false;

            case 'V':
                printf("%s v%s\n%s\n", RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR, RTP_VIEWER_COPYRIGHT);
                *exit_ok = true;
                return false;

            case '?':
            default: app_config_print_usage(argv[ 0 ]); return false;
        }
    }

    if (optind < argc)
    {
        fprintf(stderr, "Error: неизвестный аргумент: '%s'\n", argv[ optind ]);
        app_config_print_usage(argv[ 0 ]);
        return false;
    }

    if (cfg->width == 0 || cfg->height == 0 || cfg->format == nullptr)
    {
        fprintf(stderr, "Error: -x, -y и -f обязательные аргументы\n");
        app_config_print_usage(argv[ 0 ]);
        return false;
    }

    cfg->use_file_source = (cfg->source_file != nullptr);

    if (!cfg->use_file_source && cfg->port == 0)
    {
        fprintf(stderr, "Error: -p обязателен, если не задан -i\n");
        app_config_print_usage(argv[ 0 ]);
        return false;
    }

    {
        char fmt_path[ 512 ];
        snprintf(fmt_path, sizeof(fmt_path), "formats/%s.fmt", cfg->format);
        if (!file_exists(fmt_path))
        {
            fprintf(stderr, "Error: Не найден файл описания формата: %s\n", fmt_path);
            return false;
        }
    }

    if (cfg->save_prefix && !dir_exists(cfg->outdir))
    {
        fprintf(stderr, "Error: Директория для сохранения файлов не существует: %s\n", cfg->outdir);
        return false;
    }

    if (cfg->logfile)
    {
        const char * slash = strrchr(cfg->logfile, '/');
#ifdef _WIN32
        const char * bslash = strrchr(cfg->logfile, '\\');
        if (bslash && (!slash || bslash > slash))
        {
            slash = bslash;
        }
#endif
        if (slash && slash != cfg->logfile)
        {
            char dirbuf[ 512 ];
            size_t n = (size_t)(slash - cfg->logfile);
            if (n >= sizeof(dirbuf))
            {
                fprintf(stderr, "Error: Слишком длинный путь к файлу лога\n");
                return false;
            }
            memcpy(dirbuf, cfg->logfile, n);
            dirbuf[ n ] = '\0';
            if (!dir_exists(dirbuf))
            {
                fprintf(stderr, "Error: Директория для сохранения лога не существует: %s\n", dirbuf);
                return false;
            }
        }
    }

    return true;
}
