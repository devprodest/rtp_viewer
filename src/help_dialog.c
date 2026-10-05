#include "help_dialog.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
#endif

static const char * HELP_TEXT =
    "RTP Viewer - Hotkeys\n"
    "\n"
    "  1 \tToggle smoothing (linear / nearest)\n"
    "  S \tSave screenshot (PNG)\n"
    "  R \tToggle recording (on / off)\n"
    "  W \tForce save accumulated frames\n"
    "  F1 \tShow this help\n"
    "\n"
    "  Double-click \tToggle fullscreen\n";

void help_dialog_show(void)
{
#if defined(_WIN32)
    MessageBoxA(nullptr, HELP_TEXT, "RTP Viewer - Help", MB_OK | MB_ICONINFORMATION);
#else
    fprintf(stderr, "%s\n", HELP_TEXT);
#endif
}
