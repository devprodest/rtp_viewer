#include "help.h"

#include "log/logger.h"
#include "util/version.h"

#include <stdio.h>
#include <stdlib.h>

#define DEFAULT_BATCH_SIZE 500
#define HELP_FILE_NAME     "rtp_viewer_help.txt"

static char g_buf[ 8192 ];

/// \brief Формирует текст справки по горячим клавишам (без CLI).
static const char * build_hotkeys_text(void)
{
    snprintf(g_buf, sizeof(g_buf),
             "%s v%s\n"
             "================================================\n\n"
             "HOTKEYS\n\n"
             "Display:\n"
             "  1        Toggle smoothing (linear / nearest)\n"
             "  2        Toggle auto-levels (stretch by ROI percentiles)\n\n"
             "Analysis:\n"
             "  F        Toggle Sobel gradient view + sharpness metrics\n"
             "  E        Toggle saturation markers (blue=under, red=over)\n"
             "  H        Toggle histogram + CDF overlay\n"
             "  Ctrl+H   Toggle histogram scale (linear / log)\n"
             "  T        Toggle statistics overlay (min/max/mean/std)\n"
             "  L        Toggle line profile (cross-section under cursor)\n"
             "  I        Toggle pixel probe (crosshair + pixel value at cursor)\n\n"
             "Reference / Diff:\n"
             "  V        Set current frame as reference\n"
             "  B        Toggle diff view (|current - ref| * gain)\n"
             "  - / =    Decrease / increase diff gain (1x..64x)\n\n"
             "Zoom / Pan / Selection:\n"
             "  Wheel    Zoom around cursor\n"
             "  Middle   Pan (drag)\n"
             "  0        Reset zoom / pan\n"
             "  LMB      Select ROI (stats, auto-levels, diff follow selection)\n"
             "  Esc      Clear selection / close menu\n\n"
             "Playback (file source):\n"
             "  Space    Pause / resume display and analysis\n"
             "  Left     Step prev frame\n"
             "  Right    Step next frame\n"
             "  Bar      Click or drag to seek\n\n"
             "Recording / Saving:\n"
             "  S        Save screenshot (PNG)\n"
             "  R        Toggle recording (on / off)\n"
             "  W        Force save accumulated frames\n\n"
             "  F1       Save this help to file and open it\n"
             "  F11      Toggle fullscreen\n",
             RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR);

    return g_buf;
}

/// \brief Формирует текст справки по CLI-аргументам.
static const char * build_cli_text(const char * prog)
{
    snprintf(g_buf, sizeof(g_buf),
             "================================================\n"
             "COMMAND-LINE OPTIONS\n\n"
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
             "  -V, --version       Show program version\n",
             prog, DEFAULT_BATCH_SIZE);

    return g_buf;
}

void help_dialog_show(void)
{
    FILE * f = fopen(HELP_FILE_NAME, "w");
    if (!f)
    {
        log_error("help: failed to create %s", HELP_FILE_NAME);
        return;
    }

    fprintf(f, "%s\n\n", build_hotkeys_text());
    fprintf(f, "%s", build_cli_text("rtp_viewer"));

    fclose(f);
    log_info("help: saved to %s", HELP_FILE_NAME);

#ifdef _WIN32
    char cmd[ 512 ];
    snprintf(cmd, sizeof(cmd), "start \"\" \"%s\"", HELP_FILE_NAME);
    system(cmd);
#elif defined(__APPLE__)
    char cmd[ 256 ];
    snprintf(cmd, sizeof(cmd), "open %s", HELP_FILE_NAME);
    system(cmd);
#else
    char cmd[ 256 ];
    snprintf(cmd, sizeof(cmd), "xdg-open %s", HELP_FILE_NAME);
    system(cmd);
#endif
}

void help_print(const char * prog)
{
    printf("%s", build_cli_text(prog));
}
