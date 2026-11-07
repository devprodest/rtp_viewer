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
#include "save/screenshot.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdbool.h>
#include <stddef.h>

// clang-format off
static const command_info_s g_commands[] = {
    // label              hotkey    key                mods              type              group
    [CMD_SMOOTHING_TOGGLE]       = {"Smoothing",       "1",      GLFW_KEY_1,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_DISPLAY  },
    [CMD_AUTO_LEVELS_TOGGLE]     = {"Auto-levels",     "2",      GLFW_KEY_2,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_DISPLAY  },
    [CMD_GRADIENT_TOGGLE]        = {"Sobel Mask",      "F",      GLFW_KEY_F,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_INSPECTOR_TOGGLE]       = {"Inspector",       "I",      GLFW_KEY_I,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_SATURATION_TOGGLE]      = {"Saturation",      "E",      GLFW_KEY_E,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_HISTOGRAM_TOGGLE]       = {"Histogram",       "H",      GLFW_KEY_H,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_HISTOGRAM_SCALE_TOGGLE] = {"Log Scale",       "Ctrl+H", GLFW_KEY_H,        GLFW_MOD_CONTROL, CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_STATS_TOGGLE]           = {"Stats",           "T",      GLFW_KEY_T,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_LINE_PROFILE_TOGGLE]    = {"Line Profile",    "L",      GLFW_KEY_L,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_VECTORSCOPE_TOGGLE]     = {"Vectorscope",     "M",      GLFW_KEY_M,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_ANALYSIS },
    [CMD_DIFF_SET_REFERENCE]     = {"Diff Ref",        "V",      GLFW_KEY_V,        0,                CMD_TYPE_ACTION, MENU_GROUP_DIFF     },
    [CMD_DIFF_TOGGLE]            = {"Diff View",       "B",      GLFW_KEY_B,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_DIFF     },
    [CMD_DIFF_GAIN_DOWN]         = {"Diff Gain -",     "-",      GLFW_KEY_MINUS,    0,                CMD_TYPE_ACTION, MENU_GROUP_DIFF     },
    [CMD_DIFF_GAIN_UP]           = {"Diff Gain +",     "=",      GLFW_KEY_EQUAL,    0,                CMD_TYPE_ACTION, MENU_GROUP_DIFF     },
    [CMD_PLAY_STEP_BACKWARD]     = {"Backward",        "Left",   GLFW_KEY_LEFT,     0,                CMD_TYPE_ACTION, MENU_GROUP_PLAYBACK },
    [CMD_PLAY_PAUSE_TOGGLE]      = {"Pause",           "Space",  GLFW_KEY_SPACE,    0,                CMD_TYPE_TOGGLE, MENU_GROUP_PLAYBACK },
    [CMD_PLAY_STEP_FORWARD]      = {"Forward",         "Right",  GLFW_KEY_RIGHT,    0,                CMD_TYPE_ACTION, MENU_GROUP_PLAYBACK },
    [CMD_RECORD_TOGGLE]          = {"Recording",       "R",      GLFW_KEY_R,        0,                CMD_TYPE_TOGGLE, MENU_GROUP_RECORDING},
    [CMD_RECORD_FLUSH]           = {"Flush Save",      "W",      GLFW_KEY_W,        0,                CMD_TYPE_ACTION, MENU_GROUP_RECORDING},
    [CMD_RECORD_SCREENSHOT]      = {"Screenshot",      "S",      GLFW_KEY_S,        0,                CMD_TYPE_ACTION, MENU_GROUP_RECORDING},
    [CMD_RESET_ALL]              = {"Reset All",       "Esc",    GLFW_KEY_ESCAPE,   0,                CMD_TYPE_ACTION, MENU_GROUP_UTILITY},
    [CMD_ZOOM_RESET]             = {"Reset Zoom",      "0",      GLFW_KEY_0,        0,                CMD_TYPE_ACTION, MENU_GROUP_UTILITY  },
    [CMD_FULLSCREEN_TOGGLE]      = {"Full screen",     "F11",    GLFW_KEY_F11,      0,                CMD_TYPE_TOGGLE, MENU_GROUP_UTILITY  },
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
        case CMD_SMOOTHING_TOGGLE: renderer_toggle_smoothing(); break;
        case CMD_FULLSCREEN_TOGGLE: window_toggle_fullscreen(); break;
        case CMD_GRADIENT_TOGGLE: post_pass_toggle_gradient(); break;
        case CMD_INSPECTOR_TOGGLE: pixel_probe_toggle(); break;
        case CMD_SATURATION_TOGGLE: post_pass_toggle_saturation(); break;
        case CMD_HISTOGRAM_TOGGLE: histogram_view_toggle_enabled(); break;
        case CMD_HISTOGRAM_SCALE_TOGGLE: histogram_view_toggle_log_scale(); break;
        case CMD_STATS_TOGGLE: analytics_toggle_stats_visible(); break;
        case CMD_VECTORSCOPE_TOGGLE: vectorscope_view_toggle_enabled(); break;
        case CMD_LINE_PROFILE_TOGGLE: line_profile_view_toggle(); break;

        case CMD_DIFF_SET_REFERENCE: diff_view_set_reference(); break;
        case CMD_DIFF_TOGGLE: diff_view_toggle(); break;
        case CMD_DIFF_GAIN_UP: diff_view_gain_up(); break;
        case CMD_DIFF_GAIN_DOWN: diff_view_gain_down(); break;

        case CMD_AUTO_LEVELS_TOGGLE: post_pass_toggle_auto_levels(); break;
        case CMD_RECORD_FLUSH: saver_flush(); break;
        case CMD_RECORD_TOGGLE:
            saver_toggle_recording();
            window_title_refresh();
            break;
        case CMD_PLAY_PAUSE_TOGGLE: app_toggle_pause(); break;
        case CMD_PLAY_STEP_FORWARD: app_step_forward(); break;
        case CMD_PLAY_STEP_BACKWARD: app_step_backward(); break;
        case CMD_RECORD_SCREENSHOT: screenshot_request(); break;
        case CMD_RESET_ALL:
        {
            if (histogram_view_is_enabled())
            {
                histogram_view_toggle_enabled();
            }
            if (histogram_view_is_log_scale())
            {
                histogram_view_toggle_log_scale();
            }
            if (vectorscope_view_is_enabled())
            {
                vectorscope_view_toggle_enabled();
            }
            if (line_profile_view_is_enabled())
            {
                line_profile_view_toggle();
            }
            if (pixel_probe_is_enabled())
            {
                pixel_probe_toggle();
            }
            if (analytics_is_stats_visible())
            {
                analytics_toggle_stats_visible();
            }
            if (post_pass_is_gradient_active())
            {
                post_pass_toggle_gradient();
            }
            if (post_pass_is_saturation_active())
            {
                post_pass_toggle_saturation();
            }
            if (post_pass_is_auto_levels_active())
            {
                post_pass_toggle_auto_levels();
            }
            if (diff_view_is_enabled())
            {
                diff_view_toggle();
            }

            renderer_clear_selection();
            if (window_is_fullscreen())
            {
                window_toggle_fullscreen();
            }
            window_title_refresh();
        }
        break;
        case CMD_ZOOM_RESET: renderer_zoom_reset(); break;
        case CMD_SHOW_HELP: help_save_and_open(); break;
        default: break;
    }
}

bool cmd_is_active(commands_e cmd)
{
    switch (cmd)
    {
        case CMD_SMOOTHING_TOGGLE: return renderer_is_smoothing_active();
        case CMD_FULLSCREEN_TOGGLE: return window_is_fullscreen();
        case CMD_GRADIENT_TOGGLE: return post_pass_is_gradient_active();
        case CMD_INSPECTOR_TOGGLE: return pixel_probe_is_enabled();
        case CMD_SATURATION_TOGGLE: return post_pass_is_saturation_active();
        case CMD_HISTOGRAM_TOGGLE: return histogram_view_is_enabled();
        case CMD_HISTOGRAM_SCALE_TOGGLE: return histogram_view_is_log_scale();
        case CMD_STATS_TOGGLE: return analytics_is_stats_visible();
        case CMD_VECTORSCOPE_TOGGLE: return vectorscope_view_is_enabled();
        case CMD_LINE_PROFILE_TOGGLE: return line_profile_view_is_enabled();
        case CMD_AUTO_LEVELS_TOGGLE: return post_pass_is_auto_levels_active();
        case CMD_DIFF_TOGGLE: return diff_view_is_enabled();
        case CMD_RECORD_TOGGLE: return saver_is_recording();
        case CMD_PLAY_PAUSE_TOGGLE: return app_is_paused();
        default: return false;
    }
}
