#include "help.h"

#include "log/logger.h"
#include "util/version.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
#endif

#define DEFAULT_BATCH_SIZE 500

static char g_buf[ 2048 ];

/// \brief Формирует текст справки по горячим клавишам.
/// \brief Формирует текст справки по горячим клавишам.
static const char * build_hotkeys_text(void)
{
    snprintf(g_buf, sizeof(g_buf),
             "%s v%s\n"
             "\n"
             "Display:\n"
             "  1 \tToggle smoothing (linear / nearest)\n"
             "  2 \tToggle auto-levels (stretch by ROI min/max)\n"
             "  F \tToggle gradient view (Sobel) + sharpness metrics\n"
             "  E \tToggle saturation markers (blue=under, red=over)\n"
             "  H \tToggle histogram overlay\n"
             "  Ctrl+H \tToggle histogram scale (linear / log)\n"
             "  C \tToggle histogram CDF overlay\n"
             "  T \tToggle statistics overlay (min/max/mean/std)\n"
             "  X \tToggle crosshair + cursor coordinates\n"
             "  I \tToggle pixel inspector\n"
             "\n"
             "Zoom / pan:\n"
             "  Wheel \tZoom around cursor\n"
             "  Middle \tPan (drag)\n"
             "  0 \tReset zoom / pan\n"
             "\n"
             "Selection:\n"
             "  LMB \tSelect ROI (stats, auto-levels follow selection)\n"
             "  Esc \tClear selection / close menu\n"
             "\n"
             "File source:\n"
             "  Space \tPause/resume display and analysis\n"
             "  Left/Right \tStep prev/next frame\n"
             "  Progress bar \tClick or drag to seek (visible 3s after mouse motion)\n"
             "\n"
             "Recording / saving:\n"
             "  S \tSave screenshot (PNG)\n"
             "  R \tToggle recording (on / off)\n"
             "  W \tForce save accumulated frames\n"
             "\n"
             "  F1 \tShow this help\n"
             "  F11 \tToggle fullscreen\n"
             "\n"
             "CLI options: run %s with -h\n",
             RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR, "rtp_viewer");

    return g_buf;
}

/// \brief Формирует текст справки по CLI-аргументам.
static const char * build_cli_text(const char * prog)
{
    snprintf(g_buf, sizeof(g_buf),
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
             "Press F1 in the running program for hotkey help.\n",
             RTP_VIEWER_PRODUCT_NAME, RTP_VIEWER_VERSION_STR, prog, DEFAULT_BATCH_SIZE);

    return g_buf;
}

void help_dialog_show(void)
{
    const char * text = build_hotkeys_text();

#if defined(_WIN32)
    MessageBoxA(nullptr, text, "RTP Viewer - Help", MB_OK | MB_ICONINFORMATION);
#else
    log_info("%s", text);
#endif
}

void help_print(const char * prog)
{
    printf("%s", build_cli_text(prog));
}
