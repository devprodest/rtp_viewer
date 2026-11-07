#include "commands.h"

#include "analysis/analytics.h"
#include "app/app.h"
#include "help/help.h"
#include "render/histogram_view.h"
#include "render/post_pass.h"
#include "render/renderer.h"
#include "render/ui/diff_view.h"
#include "render/ui/line_profile_view.h"
#include "render/ui/pixel_probe.h"
#include "render/ui/vectorscope_view.h"
#include "render/ui/window_title.h"
#include "render/window.h"
#include "save/saver.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stddef.h>

// clang-format off
static const command_info_s g_commands[] = {
    // label              hotkey    key                mods              type              group
    [CMD_TOGGLE_SMOOTHING]       = {"Smoothing",       "1",      GLFW_KEY_1,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_DISPLAY  },
    [CMD_TOGGLE_AUTO_LEVELS]     = {"Auto-levels",     "2",      GLFW_KEY_2,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_DISPLAY  },
    [CMD_TOGGLE_GRADIENT]        = {"Sobel Mask",      "F",      GLFW_KEY_F,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_TOGGLE_INSPECTOR]       = {"Inspector",       "I",      GLFW_KEY_I,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_TOGGLE_SATURATION]      = {"Saturation",      "E",      GLFW_KEY_E,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_TOGGLE_HISTOGRAM]       = {"Histogram",       "H",      GLFW_KEY_H,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_TOGGLE_HISTOGRAM_SCALE] = {"Log Scale",       "Ctrl+H", GLFW_KEY_H,        GLFW_MOD_CONTROL, CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_TOGGLE_STATS]           = {"Stats",           "T",      GLFW_KEY_T,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_TOGGLE_LINE_PROFILE]    = {"Line Profile",    "L",      GLFW_KEY_L,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_TOGGLE_VECTORSCOPE]     = {"Vectorscope",     "M",      GLFW_KEY_M,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_SET_REFERENCE]          = {"Diff Ref",        "V",      GLFW_KEY_V,        0,                CMD_TYPE_ACTION, MENU_GROUP_DIFF     },
    [CMD_TOGGLE_DIFF]            = {"Diff View",       "B",      GLFW_KEY_B,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_DIFF     },
    [CMD_DIFF_GAIN_DOWN]         = {"Diff Gain -",     "-",      GLFW_KEY_MINUS,    0,                CMD_TYPE_ACTION, MENU_GROUP_DIFF     },
    [CMD_DIFF_GAIN_UP]           = {"Diff Gain +",     "=",      GLFW_KEY_EQUAL,    0,                CMD_TYPE_ACTION, MENU_GROUP_DIFF     },
    [CMD_STEP_BACKWARD]          = {"Backward",        "Left",   GLFW_KEY_LEFT,     0,                CMD_TYPE_ACTION, MENU_GROUP_PLAYBACK },
    [CMD_TOGGLE_PAUSE]           = {"Pause",           "Space",  GLFW_KEY_SPACE,    0,                CMD_TYPE_TOGGLE, MENU_GROUP_PLAYBACK },
    [CMD_STEP_FORWARD]           = {"Forward",         "Right",  GLFW_KEY_RIGHT,    0,                CMD_TYPE_ACTION, MENU_GROUP_PLAYBACK },
    [CMD_TOGGLE_RECORDING]       = {"Recording",       "R",      GLFW_KEY_R,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_RECORDING},
    [CMD_FLUSH_SAVER]            = {"Flush Save",      "W",      GLFW_KEY_W,        0,                CMD_TYPE_ACTION, MENU_GROUP_RECORDING},
    [CMD_REQUEST_SCREENSHOT]     = {"Screenshot",      "S",      GLFW_KEY_S,        0,                CMD_TYPE_ACTION, MENU_GROUP_RECORDING},
    [CMD_CLEAR_SELECTION]        = {"Clear Selection", "Esc",    GLFW_KEY_ESCAPE,   0,                CMD_TYPE_ACTION, MENU_GROUP_UTILITY  },
    [CMD_ZOOM_RESET]             = {"Reset Zoom",      "0",      GLFW_KEY_0,        0,                CMD_TYPE_ACTION, MENU_GROUP_UTILITY  },
    [CMD_TOGGLE_FULLSCREEN]      = {"Full screen",     "F11",    GLFW_KEY_F11,      0,                CMD_TYPE_TOGGLE, MENU_GROUP_UTILITY  },
    [CMD_SHOW_HELP]              = {"Help",            "F1",     GLFW_KEY_F1,       0,                CMD_TYPE_ACTION, MENU_GROUP_UTILITY  },
};
// clang-format on

#define CMD_ARRAY_SIZE ((int)(sizeof(g_commands) / sizeof(g_commands[ 0 ])))

const command_info_s * cmd_get_all(int * out_count)
{
    if (out_count)
    {
        *out_count = CMD_ARRAY_SIZE;
    }
    return g_commands;
}

const command_info_s * cmd_get_info(commands_e cmd)
{
    if ((int)cmd < 0 || (int)cmd >= CMD_ARRAY_SIZE)
    {
        return nullptr;
    }
    return &g_commands[ cmd ];
}

void cmd_execute(commands_e cmd)
{
    switch (cmd)
    {
        case CMD_TOGGLE_SMOOTHING: renderer_toggle_smoothing(); break;
        case CMD_TOGGLE_FULLSCREEN: window_toggle_fullscreen(); break;
        case CMD_TOGGLE_GRADIENT:
            post_pass_toggle_gradient();
            window_title_refresh();
            break;
        case CMD_TOGGLE_INSPECTOR: pixel_probe_toggle(); break;
        case CMD_TOGGLE_SATURATION:
            post_pass_toggle_saturation();
            window_title_refresh();
            break;
        case CMD_TOGGLE_HISTOGRAM:
            histogram_view_toggle_enabled();
            window_title_refresh();
            break;
        case CMD_TOGGLE_HISTOGRAM_SCALE: histogram_view_toggle_log_scale(); break;
        case CMD_TOGGLE_STATS: analytics_toggle_stats_visible(); break;
        case CMD_TOGGLE_VECTORSCOPE: vectorscope_view_toggle_enabled(); break;
        case CMD_TOGGLE_LINE_PROFILE: line_profile_view_toggle(); break;
        case CMD_SET_REFERENCE: diff_view_set_reference(); break;
        case CMD_TOGGLE_DIFF: diff_view_toggle(); break;
        case CMD_DIFF_GAIN_UP: diff_view_gain_up(); break;
        case CMD_DIFF_GAIN_DOWN: diff_view_gain_down(); break;
        case CMD_TOGGLE_AUTO_LEVELS: post_pass_toggle_auto_levels(); break;
        case CMD_FLUSH_SAVER: saver_flush(); break;
        case CMD_TOGGLE_RECORDING:
            saver_toggle_recording();
            window_title_refresh();
            break;
        case CMD_TOGGLE_PAUSE: app_toggle_pause(); break;
        case CMD_STEP_FORWARD: app_step_forward(); break;
        case CMD_STEP_BACKWARD: app_step_backward(); break;
        case CMD_REQUEST_SCREENSHOT: renderer_request_screenshot(); break;
        case CMD_CLEAR_SELECTION: renderer_clear_selection(); break;
        case CMD_ZOOM_RESET: renderer_zoom_reset(); break;
        case CMD_SHOW_HELP: help_save_and_open(); break;
        default: break;
    }
}

bool cmd_is_active(commands_e cmd)
{
    switch (cmd)
    {
        case CMD_TOGGLE_SMOOTHING: return renderer_is_smoothing_active();
        case CMD_TOGGLE_FULLSCREEN: return window_is_fullscreen();
        case CMD_TOGGLE_GRADIENT: return post_pass_is_gradient_active();
        case CMD_TOGGLE_INSPECTOR: return pixel_probe_is_enabled();
        case CMD_TOGGLE_SATURATION: return post_pass_is_saturation_active();
        case CMD_TOGGLE_HISTOGRAM: return histogram_view_is_enabled();
        case CMD_TOGGLE_HISTOGRAM_SCALE: return histogram_view_is_log_scale();
        case CMD_TOGGLE_STATS: return analytics_is_stats_visible();
        case CMD_TOGGLE_VECTORSCOPE: return vectorscope_view_is_enabled();
        case CMD_TOGGLE_LINE_PROFILE: return line_profile_view_is_enabled();
        case CMD_TOGGLE_AUTO_LEVELS: return post_pass_is_auto_levels_active();
        case CMD_TOGGLE_DIFF: return diff_view_is_enabled();
        case CMD_TOGGLE_RECORDING: return saver_is_recording();
        case CMD_TOGGLE_PAUSE: return app_is_paused();
        default: return false;
    }
}
