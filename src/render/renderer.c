#include "renderer.h"

#include "analysis/analytics.h"
#include "analysis/selection.h"
#include "format/format_config.h"
#include "format_pass.h"
#include "gl_api.h"
#include "gl_utils.h"
#include "histogram_view.h"
#include "log/logger.h"
#include "log/stats.h"
#include "post_pass.h"
#include "render/selection_view.h"
#include "save/saver.h"
#include "save/screenshot.h"
#include "text_overlay.h"
#include "window.h"

#include <stdio.h>
#include <string.h>

static GLuint g_quad_vao = 0;

static int g_width      = 0;
static int g_height     = 0;
static int g_eff_bits   = 16;
static bool g_smoothing = false;
static bool g_paused    = false;

static bool g_saturation_enabled = false;

static char g_format_name[ 64 ] = {0};

static format_config_s g_format;
static selection_s g_selection;

static int g_vp_x = 0;
static int g_vp_y = 0;
static int g_vp_w = 0;
static int g_vp_h = 0;

static bool g_left_was_down  = false;
static bool g_right_was_down = false;


static void apply_smoothing(void)
{
    GLuint tex = format_pass_get_output_tex();
    if (!tex)
    {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    GLint f = g_smoothing ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
}

void renderer_refresh_title(void)
{
    if (!window_get())
    {
        return;
    }

    char title[ 160 ];

    snprintf(title, sizeof(title), "RTP Viewer - %dx%d %s fps:%.1f%s%s", g_width, g_height,
             g_format_name[ 0 ] ? g_format_name : "?", stats_get_fps(), g_paused ? " [PAUSE]" : "",
             saver_is_recording() ? " [REC]" : "");

    window_set_title(title);
}

bool renderer_init(int width, int height, const char * format_name, int effective_bits, bool smooth_start)
{
    if (!window_init(width, height, "RTP Viewer"))
    {
        return false;
    }

    if (gl_api_load() != 0)
    {
        log_error("Failed to load OpenGL functions\n");
        window_cleanup();
        return false;
    }

    if (!format_config_load(format_name, &g_format))
    {
        window_cleanup();
        return false;
    }

    selection_init(&g_selection);
    g_left_was_down  = false;
    g_right_was_down = false;

    text_overlay_init();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    g_width    = width;
    g_height   = height;
    g_eff_bits = effective_bits;
    snprintf(g_format_name, sizeof(g_format_name), "%s", format_name);

    g_quad_vao = gl_utils_create_fullscreen_quad();
    if (g_quad_vao == 0)
    {
        log_error("renderer: failed to create fullscreen quad VAO\n");
        text_overlay_cleanup();
        window_cleanup();
        return false;
    }

    if (!format_pass_init(width, height, g_quad_vao, effective_bits, &g_format))
    {
        text_overlay_cleanup();
        window_cleanup();
        return false;
    }

    if (!post_pass_init(g_quad_vao))
    {
        format_pass_cleanup();
        text_overlay_cleanup();
        window_cleanup();
        return false;
    }

    if (!histogram_view_init(g_quad_vao))
    {
        post_pass_cleanup();
        format_pass_cleanup();
        text_overlay_cleanup();
        window_cleanup();
        return false;
    }

    screenshot_init();
    screenshot_set_size(width, height);

    g_smoothing = !smooth_start;
    renderer_toggle_smoothing();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;
}

void renderer_cleanup(void)
{
    screenshot_cleanup();
    histogram_view_cleanup();
    post_pass_cleanup();
    format_pass_cleanup();

    if (g_quad_vao)
    {
        glDeleteVertexArrays(1, &g_quad_vao);
        g_quad_vao = 0;
    }

    text_overlay_cleanup();
    window_cleanup();

    g_width  = 0;
    g_height = 0;
}

GLFWwindow * renderer_get_window(void)
{
    return window_get();
}

void renderer_poll_events(void)
{
    window_poll_events();
}

void renderer_update_texture(const uint8_t * data)
{
    format_pass_upload(data);

    analytics_update(data, g_width, g_height, &g_format, g_format_name, &g_selection);
    if (histogram_view_is_enabled())
    {
        histogram_view_update();
    }

    int win_w = 0;
    int win_h = 0;
    window_get_framebuffer_size(&win_w, &win_h);

    if (win_w <= 0 || win_h <= 0)
    {
        return;
    }

    format_pass_render_to_fbo();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    float aspect_img = (float)g_width / (float)g_height;
    float aspect_win = (float)win_w / (float)win_h;

    int vp_w = 0, vp_h = 0, vp_x = 0, vp_y = 0;
    if (aspect_win > aspect_img)
    {
        vp_h = win_h;
        vp_w = (int)(win_h * aspect_img);
        vp_x = (win_w - vp_w) / 2;
    }
    else
    {
        vp_w = win_w;
        vp_h = (int)(win_w / aspect_img);
        vp_y = (win_h - vp_h) / 2;
    }

    g_vp_x = vp_x;
    g_vp_y = vp_y;
    g_vp_w = vp_w;
    g_vp_h = vp_h;

    glViewport(0, 0, win_w, win_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    post_pass_render(format_pass_get_output_tex(), vp_x, vp_y, vp_w, vp_h);

    if (histogram_view_is_enabled() && histogram_view_get_active_channels() > 0)
    {
        const int ov_w   = 320;
        const int ov_h   = 240;
        const int margin = 10;

        int ov_x = win_w - ov_w - margin;
        int ov_y = win_h - ov_h - margin;
        if (ov_x < 0)
        {
            ov_x = 0;
        }
        if (ov_y < 0)
        {
            ov_y = 0;
        }

        histogram_view_render(ov_x, ov_y, ov_w, ov_h);
    }

    text_begin(win_w, win_h);
    selection_view_draw(&g_selection, g_vp_x, g_vp_y, g_vp_w, g_vp_h, g_width, g_height);
    text_end();

    screenshot_process_if_requested();

    window_swap_buffers();
}

int renderer_get_bytes_per_pixel(void)
{
    return format_pass_get_bytes_per_pixel();
}

void renderer_set_demosaic_mode(int mode)
{
    (void)mode;
}

void renderer_set_paused(bool paused)
{
    g_paused = paused;
    renderer_refresh_title();
}

void renderer_toggle_fullscreen(void)
{
    window_toggle_fullscreen();
}

void renderer_toggle_smoothing(void)
{
    g_smoothing = !g_smoothing;
    apply_smoothing();
    renderer_refresh_title();
    log_info("Smoothing %s", g_smoothing ? "enabled" : "disabled");
}

void renderer_toggle_histogram(void)
{
    histogram_view_toggle_enabled();
    renderer_refresh_title();
}

void renderer_toggle_histogram_scale(void)
{
    histogram_view_toggle_log_scale();
}

void renderer_toggle_histogram_cdf(void)
{
    histogram_view_toggle_cdf();
}

void renderer_toggle_saturation(void)
{
    g_saturation_enabled = !g_saturation_enabled;
    post_pass_set_saturation(g_saturation_enabled);
    renderer_refresh_title();
    log_info("Saturation markers %s", g_saturation_enabled ? "enabled" : "disabled");
}

void renderer_request_screenshot(void)
{
    screenshot_request();
}

bool renderer_screen_to_frame(double mx, double my, int * fx, int * fy)
{
    if (g_vp_w <= 0 || g_vp_h <= 0)
    {
        return false;
    }

    if (mx < g_vp_x || mx >= g_vp_x + g_vp_w || my < g_vp_y || my >= g_vp_y + g_vp_h)
    {
        return false;
    }

    int wx = (int)mx - g_vp_x;
    int wy = (int)my - g_vp_y;

    int frame_x = wx * g_width / g_vp_w;
    int frame_y = wy * g_height / g_vp_h;

    if (frame_x < 0)
    {
        frame_x = 0;
    }
    if (frame_x >= g_width)
    {
        frame_x = g_width - 1;
    }
    if (frame_y < 0)
    {
        frame_y = 0;
    }
    if (frame_y >= g_height)
    {
        frame_y = g_height - 1;
    }

    *fx = frame_x;
    *fy = frame_y;
    return true;
}

void renderer_set_selection_from_mouse(int frame_x, int frame_y, bool left_down, bool right_down, bool in_view)
{
    if (left_down && !g_left_was_down && in_view)
    {
        selection_begin(&g_selection, frame_x, frame_y);
    }
    else if (!left_down && g_left_was_down)
    {
        selection_end(&g_selection);
    }
    else if (left_down && g_left_was_down && in_view)
    {
        selection_update(&g_selection, frame_x, frame_y);
    }

    if (right_down && !g_right_was_down)
    {
        selection_clear(&g_selection);
    }

    g_left_was_down  = left_down;
    g_right_was_down = right_down;
}

void renderer_clear_selection(void)
{
    selection_clear(&g_selection);
}
