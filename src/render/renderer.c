#include "renderer.h"

#include "analysis/analytics.h"
#include "analysis/selection.h"
#include "analysis/sharpness.h"
#include "format/format_config.h"
#include "format_pass.h"
#include "gl/gl_api.h"
#include "gl/gl_utils.h"
#include "log/logger.h"
#include "log/stats.h"
#include "overlay.h"
#include "post_pass.h"
#include "save/saver.h"
#include "save/screenshot.h"
#include "ui/context_menu.h"
#include "ui/diff_view.h"
#include "ui/histogram_view.h"
#include "ui/line_profile_view.h"
#include "ui/pixel_probe.h"
#include "ui/progress_bar.h"
#include "ui/selection_view.h"
#include "ui/svg_icons.h"
#include "ui/ui_color_scheme.h"
#include "ui/vectorscope_view.h"
#include "ui/window_title.h"
#include "util/utils.h"
#include "viewport.h"
#include "window.h"

#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------------------------------------------------

static void render_stats_overlay(void);

//----------------------------------------------------------------------------------------------------------------------

static GLuint g_quad_vao = 0;

static image_size_s g_win = {};
static int g_eff_bits     = 16;
static bool g_smoothing   = false;

static format_config_s g_format;
static selection_s g_selection;

static float g_dpi_scale = 1.0f;

static bool g_left_was_down = false;

static const uint8_t * g_last_frame = nullptr;
static sharpness_s g_sharpness;

//----------------------------------------------------------------------------------------------------------------------

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

bool renderer_init(image_size_s win, const char * format_name, int effective_bits, bool smooth_start)
{
    if (!window_init(win.x, win.y, "RTP Viewer"))
    {
        return false;
    }

    {
        float sx = 1.0f;
        float sy = 1.0f;
        glfwGetWindowContentScale(window_get(), &sx, &sy);
        g_dpi_scale = (sx > 0.0f) ? sx : 1.0f;
        log_info("DPI scale: %.2f", (double)g_dpi_scale);
    }

    if (gl_api_load() != 0)
    {
        log_error("renderer: Failed to load OpenGL functions");
        goto fail_window;
    }

    if (!format_config_load(format_name, &g_format))
    {
        goto fail_window;
    }

    selection_init(&g_selection);
    g_left_was_down = false;

    overlay_init();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    g_win = win;
    viewport_init(win);
    g_eff_bits = effective_bits;

    g_quad_vao = gl_utils_create_fullscreen_quad();
    if (g_quad_vao == 0)
    {
        log_error("renderer: failed to create fullscreen quad VAO");
        goto fail_overlay;
    }

    if (!format_pass_init(win.x, win.y, g_quad_vao, effective_bits, &g_format))
    {
        log_error("renderer: failed to initialize format pass");
        goto fail_quad;
    }

    if (!post_pass_init(g_quad_vao))
    {
        log_error("renderer: failed to initialize post pass");
        goto fail_format;
    }

    post_pass_set_format(g_format.bytes_per_pixel, effective_bits);
    post_pass_set_texel_size((fimage_size_s) {1.0f / (float)g_win.x, 1.0f / (float)g_win.y});

    if (!histogram_view_init(g_quad_vao))
    {
        log_error("renderer: failed to initialize histogram view");
        goto fail_post;
    }

    if (!vectorscope_view_init(g_quad_vao))
    {
        log_error("renderer: failed to initialize vectorscope view");
        goto fail_hist;
    }

    screenshot_init();
    screenshot_set_size(win.x, win.y);

    g_smoothing = !smooth_start;
    renderer_toggle_smoothing();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return true;

    // --- Cleanup on failure (reverse order of init) ---

fail_hist:
    histogram_view_cleanup();
fail_post:
    post_pass_cleanup();
fail_format:
    format_pass_cleanup();
fail_quad:
    if (g_quad_vao)
    {
        glDeleteVertexArrays(1, &g_quad_vao);
        g_quad_vao = 0;
    }
fail_overlay:
    overlay_cleanup();
fail_window:
    window_cleanup();
    return false;
}

void renderer_cleanup(void)
{
    screenshot_cleanup();
    histogram_view_cleanup();
    vectorscope_view_cleanup();
    post_pass_cleanup();
    format_pass_cleanup();
    diff_view_cleanup();

    if (g_quad_vao)
    {
        glDeleteVertexArrays(1, &g_quad_vao);
        g_quad_vao = 0;
    }

    overlay_cleanup();
    window_cleanup();

    g_win.x = 0;
    g_win.y = 0;
}

void renderer_set_frame(const uint8_t * data)
{
    g_last_frame = data;

    const uint8_t * frame_to_upload = diff_view_process(data, g_win, g_format.bytes_per_pixel, g_eff_bits);
    format_pass_upload(frame_to_upload);

    analytics_update(data, g_win, &g_format, &g_selection, g_eff_bits);
    pixel_probe_set_frame(data, g_win, g_format.bytes_per_pixel);

    if (post_pass_is_gradient_active())
    {
        g_sharpness = sharpness_compute(data, g_win, &g_format, &g_selection);
        window_title_set_sharpness(g_sharpness.valid, g_sharpness.lap_var, g_sharpness.tenengrad);
    }

    histogram_view_update(g_eff_bits);
    vectorscope_view_update(data, g_win, &g_format, &g_selection, g_format.bytes_per_pixel);
}

void renderer_draw(void)
{
    image_size_s win = window_get_framebuffer_size();
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }

    format_pass_render_to_fbo();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    viewport_update(win);

    glViewport(0, 0, win.x, win.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    post_pass_render(format_pass_get_output_tex(), viewport_get_point(), viewport_get_size(), win);

    histogram_view_render(win);
    vectorscope_view_render(win, g_dpi_scale);

    overlay_begin(win);
    selection_view_draw(&g_selection, viewport_get_point(), viewport_get_size(), g_win);
    pixel_probe_draw(win, g_dpi_scale);
    render_stats_overlay();
    line_profile_view_draw(win, g_dpi_scale);
    progress_bar_draw(win, g_dpi_scale);
    overlay_end();

    // Меню полностью автономно (внутри себя управляет оверлеями и NanoVG)
    context_menu_draw(win, g_dpi_scale);

    screenshot_process_if_requested();
    window_swap_buffers();
}

int renderer_get_bytes_per_pixel(void)
{
    return format_pass_get_bytes_per_pixel();
}

void renderer_clear_selection(void)
{
    selection_clear(&g_selection);
}
bool renderer_screen_to_frame(point_s screen, point_s * frame)
{
    return viewport_screen_to_frame(screen, frame);
}
void renderer_zoom_reset(void)
{
    viewport_reset();
}
void renderer_zoom_apply_wheel(double wheel, point_s point)
{
    viewport_apply_wheel(wheel, point);
}
void renderer_pan_apply_drag(point_s point, bool middle_down)
{
    viewport_apply_pan_drag(point, middle_down);
}
float renderer_get_dpi_scale(void)
{
    return g_dpi_scale;
}
bool renderer_is_smoothing_active(void)
{
    return g_smoothing;
}

void renderer_toggle_smoothing(void)
{
    g_smoothing = !g_smoothing;
    apply_smoothing();
    window_title_refresh();
    log_info("Smoothing %s", g_smoothing ? "enabled" : "disabled");
}

void renderer_refresh_analytics(void)
{
    if (!g_last_frame)
    {
        return;
    }

    analytics_update(g_last_frame, g_win, &g_format, &g_selection, g_eff_bits);
    pixel_probe_set_frame(g_last_frame, g_win, g_format.bytes_per_pixel);

    if (histogram_view_is_enabled())
    {
        histogram_view_update(g_eff_bits);
    }
    vectorscope_view_update(g_last_frame, g_win, &g_format, &g_selection, g_format.bytes_per_pixel);
}

void renderer_set_selection_from_mouse(point_s mouse, bool left_down, bool in_view)
{
    bool was_dragging = g_left_was_down;

    if (left_down && !g_left_was_down && in_view)
    {
        selection_begin(&g_selection, mouse);
    }
    else if (!left_down && g_left_was_down)
    {
        selection_end(&g_selection);
    }
    else if (left_down && g_left_was_down && in_view)
    {
        selection_update(&g_selection, mouse);
    }

    g_left_was_down = left_down;

    if (was_dragging && !left_down)
    {
        renderer_refresh_analytics();
    }
}

static void render_stats_overlay(void)
{
    if (!analytics_is_stats_visible())
    {
        return;
    }

    const analytics_stats_s * st = analytics_get_stats();
    if (!st || !st->valid)
    {
        return;
    }

    const float text_scale = g_dpi_scale * 1.f;
    const int margin       = (int)(10 * g_dpi_scale);
    const int line_h       = (int)(14 * text_scale);
    const int x            = margin;
    int y                  = margin;

    char line[ 192 ];

    const int panel_h = (st->channels == 1 ? 1 : 3) * line_h;
    const int panel_w = 380;
    rect_s panel      = {.x0 = x - 4, .y0 = y - 4, .x1 = x + panel_w, .y1 = y + panel_h + 4};
    overlay_rect_draw(panel, UI_PANEL_BG);

    if (st->channels == 1)
    {
        snprintf(line, sizeof(line), "RAW min=%.0f max=%.0f mean=%.1f std=%.1f", st->min[ 0 ], st->max[ 0 ],
                 st->mean[ 0 ], st->std[ 0 ]);
        overlay_text_draw((point_s) {x, y}, line, UI_TEXT_PRIMARY, text_scale);
    }
    else
    {
        static const char * names[ 3 ] = {"R", "G", "B"};
        for (int c = 0; c < 3; c++)
        {
            snprintf(line, sizeof(line), "%s min=%.0f max=%.0f mean=%.1f std=%.1f", names[ c ], st->min[ c ],
                     st->max[ c ], st->mean[ c ], st->std[ c ]);
            overlay_text_draw((point_s) {x, y + c * line_h}, line, UI_TEXT_PRIMARY, text_scale);
        }
    }
}

void renderer_set_mouse_pos(point_s mouse)
{
    pixel_probe_set_mouse(mouse);
    progress_bar_notify_mouse(mouse);
}

bool renderer_handle_context_menu(point_s mouse, bool right_down, bool left_down)
{
    static bool prev_right = false;
    static bool prev_left  = false;
    bool consumed          = false;

    if (context_menu_is_open())
    {
        context_menu_set_hover(mouse);

        if (left_down && !prev_left)
        {
            image_size_s win = window_get_framebuffer_size();
            context_menu_handle_click(mouse, win, g_dpi_scale);
            consumed = true;
        }
        else if (right_down && !prev_right)
        {
            context_menu_close();
            consumed = true;
        }
    }
    else if (right_down && !prev_right)
    {
        context_menu_open(mouse);
        consumed = true;
    }

    prev_right = right_down;
    prev_left  = left_down;
    return consumed;
}
