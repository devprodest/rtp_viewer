#include "config.h"

#include "help/help.h"
#include "log/logger.h"
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

void app_config_print_usage(const char * prog)
{
    help_print(prog);
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
                    log_error("config: -x specified more than once");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, 65535, &v) != 0)
                    {
                        log_error("config: -x expects integer in [1..65535], got '%s'", optarg);
                        return false;
                    }
                    cfg->image.x = (int)v;
                }
                seen_x = true;
                break;

            case 'y':
                if (seen_y)
                {
                    log_error("config: -y specified more than once");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, 65535, &v) != 0)
                    {
                        log_error("config: -y expects integer in [1..65535], got '%s'", optarg);
                        return false;
                    }
                    cfg->image.y = (int)v;
                }
                seen_y = true;
                break;

            case 'p':
                if (seen_p)
                {
                    log_error("config: -p specified more than once");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, 65535, &v) != 0)
                    {
                        log_error("config: -p expects UDP port in [1..65535], got '%s'", optarg);
                        return false;
                    }
                    cfg->port = (int)v;
                }
                seen_p = true;
                break;

            case 'i':
                if (seen_i)
                {
                    log_error("config: -i specified more than once");
                    return false;
                }
                if (*optarg == '\0')
                {
                    log_error("config: -i expects a path to .raw file");
                    return false;
                }
                cfg->source_file = optarg;
                seen_i           = true;
                break;

            case 'f':
                if (seen_f)
                {
                    log_error("config: -f specified more than once");
                    return false;
                }
                if (*optarg == '\0')
                {
                    log_error("config: -f expects a format name");
                    return false;
                }
                cfg->format = optarg;
                seen_f      = true;
                break;

            case 'e':
                if (seen_e)
                {
                    log_error("config: -e specified more than once");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, 16, &v) != 0)
                    {
                        log_error("config: -e expects integer in [1..16], got '%s'", optarg);
                        return false;
                    }
                    cfg->effective_bits = (int)v;
                }
                seen_e = true;
                break;

            case 's':
                if (seen_s)
                {
                    log_error("config: -s specified more than once");
                    return false;
                }
                if (*optarg == '\0')
                {
                    log_error("config: -s expects a non-empty prefix");
                    return false;
                }
                cfg->save_prefix = optarg;
                seen_s           = true;
                break;

            case 'b':
                if (seen_b)
                {
                    log_error("config: -b specified more than once");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 1, INT_MAX, &v) != 0)
                    {
                        log_error("config: -b expects integer >= 1, got '%s'", optarg);
                        return false;
                    }
                    cfg->batch_size = (int)v;
                }
                seen_b = true;
                break;

            case 't':
                if (seen_t)
                {
                    log_error("config: -t specified more than once");
                    return false;
                }
                {
                    long v = 0;
                    if (parse_int(optarg, 0, INT_MAX, &v) != 0)
                    {
                        log_error("config: -t expects integer >= 0, got '%s'", optarg);
                        return false;
                    }
                    cfg->time_window = (int)v;
                }
                seen_t = true;
                break;

            case 'l':
                if (seen_l)
                {
                    log_error("config: -l specified more than once");
                    return false;
                }
                if (*optarg == '\0')
                {
                    log_error("config: -l expects a file name");
                    return false;
                }
                cfg->logfile = optarg;
                seen_l       = true;
                break;

            case 'o':
                if (seen_o)
                {
                    log_error("config: -o specified more than once");
                    return false;
                }
                if (*optarg == '\0')
                {
                    log_error("config: -o expects a directory path");
                    return false;
                }
                cfg->outdir = optarg;
                seen_o      = true;
                break;

            case 'S': cfg->smooth_start = true; break;

            case 'h':
                help_print(argv[0]);
                *exit_ok = true;
                return false;

            case 'V':
                log_info("%s v%s", RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR);
                log_info("%s", RTP_VIEWER_COPYRIGHT);
                *exit_ok = true;
                return false;

            case '?':
            default: help_print(argv[0]); return false;
        }
    }

    if (optind < argc)
    {
        log_error("config: unknown argument '%s'", argv[optind]);
        help_print(argv[0]);
        return false;
    }

    if (cfg->image.x == 0 || cfg->image.y == 0 || cfg->format == nullptr)
    {
        log_error("config: -x, -y and -f are required");
        help_print(argv[0]);
        return false;
    }

    cfg->use_file_source = (cfg->source_file != nullptr);

    if (!cfg->use_file_source && cfg->port == 0)
    {
        log_error("config: -p is required when -i is not given");
        help_print(argv[0]);
        return false;
    }

    {
        char fmt_path[512];
        snprintf(fmt_path, sizeof(fmt_path), "formats/%s.fmt", cfg->format);
        if (!file_exists(fmt_path))
        {
            log_error("config: format file not found: %s", fmt_path);
            return false;
        }
    }

    if (cfg->save_prefix && !dir_exists(cfg->outdir))
    {
        log_error("config: output directory does not exist: %s", cfg->outdir);
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
            char dirbuf[512];
            size_t n = (size_t)(slash - cfg->logfile);
            if (n >= sizeof(dirbuf))
            {
                log_error("config: logfile path is too long");
                return false;
            }
            memcpy(dirbuf, cfg->logfile, n);
            dirbuf[n] = '\0';
            if (!dir_exists(dirbuf))
            {
                log_error("config: logfile directory does not exist: %s", dirbuf);
                return false;
            }
        }
    }

    return true;
}