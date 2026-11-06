#include "renderer.h"

#include "analysis/analytics.h"
#include "analysis/selection.h"
#include "analysis/sharpness.h"
#include "format/format_config.h"
#include "format_pass.h"
#include "gl_api.h"
#include "gl_utils.h"
#include "histogram_view.h"
#include "log/logger.h"
#include "log/stats.h"
#include "overlay.h"
#include "post_pass.h"
#include "save/saver.h"
#include "save/screenshot.h"
#include "selection_view.h"
#include "ui/context_menu.h"
#include "ui/line_profile_view.h"
#include "ui/pixel_probe.h"
#include "ui/progress_bar.h"
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

static bool g_saturation_enabled = false;

static format_config_s g_format;
static selection_s g_selection;

static float g_dpi_scale = 1.0f;

static bool g_left_was_down = false;

static const uint8_t * g_last_frame = nullptr;

static bool g_auto_levels_enabled = false;
static bool g_gradient_enabled    = false;
static sharpness_s g_sharpness;

static uint8_t * g_reference_buffer = nullptr;
static size_t g_reference_bytes     = 0;
static uint8_t * g_diff_buffer      = nullptr;
static size_t g_diff_bytes          = 0;
static bool g_diff_enabled          = false;
static float g_diff_gain            = 4.0f;

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
    g_left_was_down = false;

    overlay_init();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    g_win = win;

    viewport_init(win);

    g_eff_bits = effective_bits;

    g_quad_vao = gl_utils_create_fullscreen_quad();
    if (g_quad_vao == 0)
    {
        log_error("renderer: failed to create fullscreen quad VAO\n");
        overlay_cleanup();
        window_cleanup();
        return false;
    }

    if (!format_pass_init(win.x, win.y, g_quad_vao, effective_bits, &g_format))
    {
        overlay_cleanup();
        window_cleanup();
        return false;
    }

    if (!post_pass_init(g_quad_vao))
    {
        format_pass_cleanup();
        overlay_cleanup();
        window_cleanup();
        return false;
    }

    post_pass_set_texel_size((fimage_size_s) {1.0f / (float)g_win.x, 1.0f / (float)g_win.y});

    if (!histogram_view_init(g_quad_vao))
    {
        post_pass_cleanup();
        format_pass_cleanup();
        overlay_cleanup();
        window_cleanup();
        return false;
    }

    if (!vectorscope_view_init(g_quad_vao))
    {
        histogram_view_cleanup();
        post_pass_cleanup();
        format_pass_cleanup();
        overlay_cleanup();
        window_cleanup();
        return false;
    }

    screenshot_init();
    screenshot_set_size(win.x, win.y);

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
    vectorscope_view_cleanup();
    post_pass_cleanup();
    format_pass_cleanup();

    if (g_quad_vao)
    {
        glDeleteVertexArrays(1, &g_quad_vao);
        g_quad_vao = 0;
    }

    free(g_reference_buffer);
    g_reference_buffer = nullptr;
    g_reference_bytes  = 0;
    free(g_diff_buffer);
    g_diff_buffer  = nullptr;
    g_diff_bytes   = 0;
    g_diff_enabled = false;

    overlay_cleanup();
    window_cleanup();

    g_win.x = 0;
    g_win.y = 0;
}

GLFWwindow * renderer_get_window(void)
{
    return window_get();
}

void renderer_poll_events(void)
{
    window_poll_events();
}

static size_t current_frame_bytes(void)
{
    return g_win.x * g_win.y * g_format.bytes_per_pixel;
}

static void compute_diff(const uint8_t * cur, const uint8_t * ref, uint8_t * out, size_t bytes)
{
    const float gain = g_diff_gain;

    if (g_format.bytes_per_pixel == 2)
    {
        const uint16_t * c = (const uint16_t *)cur;
        const uint16_t * r = (const uint16_t *)ref;
        uint16_t * o       = (uint16_t *)out;
        const size_t n     = bytes / 2;
        const int maxval   = (g_eff_bits >= 16) ? 65535 : ((1 << g_eff_bits) - 1);

        for (size_t i = 0; i < n; i++)
        {
            int d  = utils_abs((int)c[ i ] - (int)r[ i ]);
            int v  = (int)((float)d * gain + 0.5f);
            o[ i ] = (uint16_t)utils_min(v, maxval);
        }
    }
    else
    {
        // gray8 / RGB / RGBA — побайтово (каждый байт — отдельный канал)
        for (size_t i = 0; i < bytes; i++)
        {
            int d    = utils_abs((int)cur[ i ] - (int)ref[ i ]);
            int v    = (int)((float)d * gain + 0.5f);
            out[ i ] = (uint8_t)utils_min(v, 255);
        }
    }
}


static void autolevel_calculate(void)
{
    if (g_auto_levels_enabled)
    {
        const analytics_stats_s * st = analytics_get_stats();
        if (st && st->valid)
        {
            const double max_range = (g_format.bytes_per_pixel == 2) ? (double)((1u << g_eff_bits) - 1u) : 255.0;

            float lmin[ 3 ] = {0.0f, 0.0f, 0.0f};
            float lmax[ 3 ] = {1.0f, 1.0f, 1.0f};

            for (int c = 0; c < 3; c++)
            {
                double lo = st->min[ c ] / max_range;
                double hi = st->max[ c ] / max_range;
                if (hi <= lo + 1e-3)
                {
                    lo = 0.0;
                    hi = 1.0;
                }
                lmin[ c ] = utils_clamp((float)lo, 0.0f, 1.0f);
                lmax[ c ] = utils_clamp((float)hi, 0.0f, 1.0f);
            }
            if (st->channels == 1)
            {
                lmin[ 1 ] = lmin[ 2 ] = lmin[ 0 ];
                lmax[ 1 ] = lmax[ 2 ] = lmax[ 0 ];
            }
            post_pass_set_levels(lmin, lmax);
        }
        else
        {
            post_pass_set_levels((float[ 3 ]) {0.f, 0.f, 0.f}, (float[ 3 ]) {1.f, 1.f, 1.f});
        }
    }
}


void renderer_set_frame(const uint8_t * data)
{
    g_last_frame = data;

    const uint8_t * frame_to_upload = data;

    // --- Diff ---
    if (g_diff_enabled && g_reference_buffer)
    {
        const size_t bytes = current_frame_bytes();
        if (bytes > 0 && bytes == g_reference_bytes)
        {
            if (g_diff_bytes != bytes)
            {
                free(g_diff_buffer);
                g_diff_buffer = (uint8_t *)malloc(bytes);
                g_diff_bytes  = g_diff_buffer ? bytes : 0;
            }
            if (g_diff_buffer)
            {
                compute_diff(data, g_reference_buffer, g_diff_buffer, bytes);
                frame_to_upload = g_diff_buffer;
            }
        }
    }

    format_pass_upload(frame_to_upload);

    // Аналитика, probe, sharpness, гистограмма — по РЕАЛЬНОМУ кадру (data),
    // не по diff. Diff — только визуальный слой.
    analytics_update(data, g_win, &g_format, &g_selection, g_eff_bits);
    pixel_probe_set_frame(data, g_win, g_format.bytes_per_pixel);

    if (g_gradient_enabled)
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

    autolevel_calculate();

    post_pass_render(format_pass_get_output_tex(), viewport_get_point(), viewport_get_size(), win);

    histogram_view_render(win);
    vectorscope_view_render(win, g_dpi_scale);

    overlay_begin(win);
    selection_view_draw(&g_selection, viewport_get_point(), viewport_get_size(), g_win);
    pixel_probe_draw(win, g_dpi_scale);
    render_stats_overlay();
    line_profile_view_draw(win, g_dpi_scale);
    progress_bar_draw(win, g_dpi_scale);
    context_menu_draw(win, g_dpi_scale);
    overlay_end();

    if (context_menu_is_open())
    {
        svg_icons_begin(win, g_dpi_scale);
        context_menu_draw_icons(win, g_dpi_scale);
        svg_icons_end();
    }

    screenshot_process_if_requested();
    window_swap_buffers();
}


int renderer_get_bytes_per_pixel(void)
{
    return format_pass_get_bytes_per_pixel();
}

void renderer_set_demosaic_mode(int /*unused*/)
{
}

void renderer_set_paused(bool paused)
{
    window_title_set_paused(paused);
}

void renderer_toggle_fullscreen(void)
{
    window_toggle_fullscreen();
}

void renderer_toggle_smoothing(void)
{
    g_smoothing = !g_smoothing;
    apply_smoothing();
    window_title_refresh();
    log_info("Smoothing %s", g_smoothing ? "enabled" : "disabled");
}

void renderer_toggle_histogram(void)
{
    histogram_view_toggle_enabled();
    window_title_refresh();
}

void renderer_toggle_histogram_scale(void)
{
    histogram_view_toggle_log_scale();
}

void renderer_toggle_saturation(void)
{
    g_saturation_enabled = !g_saturation_enabled;
    post_pass_set_saturation(g_saturation_enabled);
    window_title_refresh();
    log_info("Saturation markers %s", g_saturation_enabled ? "enabled" : "disabled");
}

void renderer_request_screenshot(void)
{
    screenshot_request();
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

    // Выделение закончилось — пересчитать аналитику по новому ROI
    if (was_dragging && !left_down)
    {
        renderer_refresh_analytics();
    }
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
    rect_s panel      = {
        .x0 = x - 4,
        .y0 = y - 4,
        .x1 = x + panel_w,
        .y1 = y + panel_h + 4,
    };
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

void renderer_toggle_gradient(void)
{
    g_gradient_enabled = !g_gradient_enabled;
    post_pass_set_gradient(g_gradient_enabled);
    if (g_gradient_enabled == false)
    {
        window_title_set_sharpness(false, 0.0, 0.0);
    }
    window_title_refresh();
    log_info("Gradient %s", g_gradient_enabled ? "enabled" : "disabled");
}

void renderer_toggle_auto_levels(void)
{
    g_auto_levels_enabled = !g_auto_levels_enabled;
    post_pass_set_auto_levels(g_auto_levels_enabled);
    log_info("Auto-levels %s", g_auto_levels_enabled ? "enabled" : "disabled");
}

bool renderer_is_auto_levels_active(void)
{
    return g_auto_levels_enabled;
}

void renderer_set_reference(void)
{
    if (!g_last_frame)
    {
        log_info("Reference: no frame available yet");
        return;
    }

    const size_t bytes = current_frame_bytes();
    if (bytes == 0)
    {
        return;
    }

    if (g_reference_bytes != bytes)
    {
        free(g_reference_buffer);
        g_reference_buffer = (uint8_t *)malloc(bytes);
        if (!g_reference_buffer)
        {
            g_reference_bytes = 0;
            log_error("Reference: out of memory (%zu bytes)", bytes);
            return;
        }
        g_reference_bytes = bytes;
    }

    memcpy(g_reference_buffer, g_last_frame, bytes);
    log_info("Reference frame set (%dx%d, %zu bytes)", g_win.x, g_win.y, bytes);
}

void renderer_toggle_diff(void)
{
    if (!g_reference_buffer)
    {
        log_info("Diff: no reference set (press V first)");
        return;
    }
    g_diff_enabled = !g_diff_enabled;
    log_info("Diff %s (gain %.0fx)", g_diff_enabled ? "enabled" : "disabled", (double)g_diff_gain);
}

void renderer_diff_gain_up(void)
{
    g_diff_gain = utils_min(g_diff_gain * 2.0f, 64.0f);
    log_info("Diff gain = %.0fx", (double)g_diff_gain);
}

void renderer_diff_gain_down(void)
{
    g_diff_gain = utils_max(g_diff_gain / 2.0f, 1.0f);
    log_info("Diff gain = %.0fx", (double)g_diff_gain);
}

bool renderer_is_diff_active(void)
{
    return g_diff_enabled;
}

bool renderer_handle_context_menu(point_s mouse, bool right_down, bool left_down)
{
    static bool prev_right = false;
    static bool prev_left  = false;

    bool consumed = false;

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

float renderer_get_dpi_scale(void)
{
    return g_dpi_scale;
}

bool renderer_is_smoothing_active(void)
{
    return g_smoothing;
}
bool renderer_is_saturation_active(void)
{
    return g_saturation_enabled;
}
bool renderer_is_gradient_active(void)
{
    return g_gradient_enabled;
}

void renderer_toggle_vectorscope(void)
{
    vectorscope_view_toggle_enabled();
    if (vectorscope_view_is_enabled() && g_last_frame)
    {
        vectorscope_view_update(g_last_frame, g_win, &g_format, &g_selection, g_format.bytes_per_pixel);
    }
}

bool renderer_is_vectorscope_active(void)
{
    return vectorscope_view_is_enabled();
}
