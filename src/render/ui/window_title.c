#include "window_title.h"

#include "log/stats.h"
#include "render/window.h"
#include "save/saver.h"

#include <stdio.h>
#include <string.h>

static char g_format_name[64] = {0};
static image_size_s g_frame   = {0};
static bool g_paused          = false;
static bool g_sharp_valid     = false;
static double g_lap_var       = 0.0;
static double g_tenengrad     = 0.0;
static int g_frame_current    = 0;
static int g_frame_total      = 0;

void window_title_init(const char * format_name, image_size_s frame_size)
{
    g_format_name[0] = '\0';
    if (format_name)
    {
        snprintf(g_format_name, sizeof(g_format_name), "%s", format_name);
    }

    g_frame         = frame_size;
    g_paused        = false;
    g_sharp_valid   = false;
    g_lap_var       = 0.0;
    g_tenengrad     = 0.0;
    g_frame_current = 0;
    g_frame_total   = 0;
}

void window_title_set_paused(bool paused)
{
    g_paused = paused;
    window_title_refresh();
}

void window_title_set_sharpness(bool valid, double lap, double ten)
{
    g_sharp_valid = valid;
    g_lap_var     = lap;
    g_tenengrad   = ten;
}

void window_title_set_frame(int current, int total)
{
    g_frame_current = current;
    g_frame_total   = total;
}

void window_title_refresh(void)
{
    if (!window_get())
    {
        return;
    }

    char frame_str[32] = "";
    if (g_frame_total > 0)
    {
        snprintf(frame_str, sizeof(frame_str), " [%d/%d]", g_frame_current + 1, g_frame_total);
    }

    char extra[64] = "";
    if (g_sharp_valid)
    {
        snprintf(extra, sizeof(extra), " Lap=%.1f Ten=%.0f", g_lap_var, g_tenengrad);
    }

    char title[256];
    snprintf(title, sizeof(title), "RTP Viewer - %dx%d %s%s fps:%.1f%s%s%s", //
             g_frame.x, g_frame.y,                                           //
             g_format_name[0] ? g_format_name : "?",                         //
             frame_str,                                                      //
             stats_get_fps(),                                                //
             g_paused ? " [PAUSE]" : "",                                     //
             saver_is_recording() ? " [REC]" : "",                           //
             extra);

    window_set_title(title);
}
