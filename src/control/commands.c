#include "commands.h"

#include "help/help.h"
#include "render/renderer.h"
#include "save/saver.h"

void cmd_toggle_smoothing(void)
{
    renderer_toggle_smoothing();
}

void cmd_toggle_saturation(void)
{
    renderer_toggle_saturation();
}

void cmd_toggle_histogram(void)
{
    renderer_toggle_histogram();
}

void cmd_toggle_histogram_scale(void)
{
    renderer_toggle_histogram_scale();
}

void cmd_toggle_histogram_cdf(void)
{
    renderer_toggle_histogram_cdf();
}

void cmd_request_screenshot(void)
{
    renderer_request_screenshot();
}

void cmd_toggle_recording(void)
{
    saver_toggle_recording();
    renderer_refresh_title();
}

void cmd_flush_saver(void)
{
    saver_flush();
}

void cmd_show_help(void)
{
    help_dialog_show();
}

void cmd_toggle_pause(void)
{
    extern bool app_toggle_pause(void);
    app_toggle_pause();
}