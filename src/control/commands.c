#include "commands.h"

#include "analysis/analytics.h"
#include "app/app.h"
#include "help/help.h"
#include "render/histogram_view.h"
#include "render/renderer.h"
#include "render/ui/line_profile_view.h"
#include "render/ui/pixel_probe.h"
#include "render/ui/window_title.h"
#include "render/window.h"
#include "save/saver.h"

#include <stdbool.h>


void cmd_execute(commands_e cmd)
{
    switch (cmd)
    {
        case CMD_TOGGLE_SMOOTHING: renderer_toggle_smoothing(); break;
        case CMD_TOGGLE_AUTO_LEVELS: renderer_toggle_auto_levels(); break;
        case CMD_TOGGLE_FULLSCREEN: window_toggle_fullscreen(); break;

        case CMD_TOGGLE_GRADIENT: renderer_toggle_gradient(); break;
        case CMD_TOGGLE_INSPECTOR: pixel_probe_toggle(); break;
        case CMD_TOGGLE_SATURATION: renderer_toggle_saturation(); break;
        case CMD_TOGGLE_HISTOGRAM: renderer_toggle_histogram(); break;
        case CMD_TOGGLE_HISTOGRAM_SCALE: renderer_toggle_histogram_scale(); break;
        case CMD_TOGGLE_LINE_PROFILE: line_profile_view_toggle(); break;
        case CMD_TOGGLE_STATS: analytics_toggle_stats_visible(); break;

        case CMD_FLUSH_SAVER: saver_flush(); break;
        case CMD_TOGGLE_RECORDING:
            saver_toggle_recording();
            window_title_refresh();
            break;

        case CMD_SET_REFERENCE: renderer_set_reference(); break;
        case CMD_TOGGLE_DIFF: renderer_toggle_diff(); break;
        case CMD_DIFF_GAIN_UP: renderer_diff_gain_up(); break;
        case CMD_DIFF_GAIN_DOWN: renderer_diff_gain_down(); break;

        case CMD_TOGGLE_PAUSE: app_toggle_pause(); break;
        case CMD_STEP_FORWARD: app_step_forward(); break;
        case CMD_STEP_BACKWARD: app_step_backward(); break;

        case CMD_REQUEST_SCREENSHOT: renderer_request_screenshot(); break;

        case CMD_CLEAR_SELECTION: renderer_clear_selection(); break;

        case CMD_ZOOM_RESET: renderer_zoom_reset(); break;

        case CMD_SHOW_HELP: help_dialog_show(); break;
        default: break;
    }
}

bool cmd_is_active(commands_e cmd)
{
    switch (cmd)
    {
        case CMD_TOGGLE_SMOOTHING: return renderer_is_smoothing_active();
        case CMD_TOGGLE_AUTO_LEVELS: return renderer_is_auto_levels_active();
        case CMD_TOGGLE_FULLSCREEN: return window_is_fullscreen();
        case CMD_TOGGLE_GRADIENT: return renderer_is_gradient_active();
        case CMD_TOGGLE_INSPECTOR: return pixel_probe_is_enabled();
        case CMD_TOGGLE_SATURATION: return renderer_is_saturation_active();
        case CMD_TOGGLE_HISTOGRAM: return histogram_view_is_enabled();
        case CMD_TOGGLE_HISTOGRAM_SCALE: return histogram_view_is_log_scale();
        case CMD_TOGGLE_LINE_PROFILE: return line_profile_view_is_enabled();
        case CMD_TOGGLE_STATS: return analytics_is_stats_visible();
        case CMD_TOGGLE_RECORDING: return saver_is_recording();
        case CMD_TOGGLE_PAUSE: return app_is_paused();
        case CMD_TOGGLE_DIFF: return renderer_is_diff_active();

        default: return false;
    }
}
