#include "help_dialog.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
#endif

static const char * HELP_BODY =
    "\n"
    "  1 \tToggle smoothing (linear / nearest)\n"
    "  S \tSave screenshot (PNG)\n"
    "  R \tToggle recording (on / off)\n"
    "  W \tForce save accumulated frames\n"
    "  H \tToggle histogram overlay\n"
    "  F1 \tShow this help\n"
    "\n"
    "  Double-click   Toggle fullscreen\n";

static char g_help_text[1024];

static const char * build_help_text(void)
{
    snprintf(g_help_text, sizeof(g_help_text), "%s v%s\n%s", //
             RTP_VIEWER_PRODUCT_NAME,
             RTP_VIEWER_VERSION_STR,                         //
             HELP_BODY);
    return g_help_text;
}

void help_dialog_show(void)
{
#if defined(_WIN32)
    MessageBoxA(nullptr, build_help_text(), "RTP Viewer - Help", MB_OK | MB_ICONINFORMATION);
#else
    fprintf(stderr, "%s\n", HELP_TEXT);
#endif
}
