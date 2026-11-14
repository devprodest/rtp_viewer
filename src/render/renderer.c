#include "renderer.h"

#include "analysis/analytics.h"
#include "analysis/flicker.h"
#include "analysis/frame_context.h"
#include "analysis/selection.h"
#include "analysis/sharpness.h"
#include "app/settings.h"
#include "format/format_config.h"
#include "format_pass.h"
#include "gl/gl_api.h"
#include "gl/gl_utils.h"
#include "log/logger.h"
#include "log/stats.h"
#include "overlay.h"
#include "post_pass.h"
#include "save/screenshot.h"
#include "ui/context_menu.h"
#include "ui/defect_view.h"
#include "ui/diff_view.h"
#include "ui/flicker_view.h"
#include "ui/histogram_view.h"
#include "ui/line_profile_view.h"
#include "ui/panel.h"
#include "ui/pixel_probe.h"
#include "ui/progress_bar.h"
#include "ui/selection_view.h"
#include "ui/stats_overlay.h"
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

//------------------------------------------------------- Обёртки для panel
//---------------------------------------------

static image_size_s histogram_get_size_wrap(float)
{
    return histogram_view_get_size();
}

static image_size_s vectorscope_get_size_wrap(float)
{
    return vectorscope_view_get_size();
}

static image_size_s flicker_get_size_wrap(float)
{
    return flicker_view_get_size();
}

//------------------------------------------------------- Smoothing ----------------------------------------------------

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

//------------------------------------------------------- Init --------------------------------------------------------

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

    g_win      = win;
    g_eff_bits = effective_bits;
    viewport_init(win);

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

    post_pass_set_format(g_format.bytes_per_pixel, effective_bits);
    post_pass_set_texel_size((fimage_size_s) {1.0f / (float)g_win.x, 1.0f / (float)g_win.y});

    flicker_init();

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
    if (!flicker_view_init(g_quad_vao))
    {
        vectorscope_view_cleanup();
        histogram_view_cleanup();
        post_pass_cleanup();
        format_pass_cleanup();
        overlay_cleanup();
        window_cleanup();
        return false;
    }

    diff_view_init();

    screenshot_init();
    screenshot_size_set(win.x, win.y);

    { // ini — база; CLI-флаг переопределяет, если ключ есть.
        bool want_smoothing;
        if (settings_get_feature_bool("smoothing", "enabled", &want_smoothing))
        {
            g_smoothing = want_smoothing;
        }
        else
        {
            g_smoothing = smooth_start;
        }
        apply_smoothing();
        log_info("Smoothing %s (init)", g_smoothing ? "enabled" : "disabled");
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- Регистрация перетаскиваемых панелей ---
    static const panel_s panels[] = {
        [0] = {.id         = "histogram",
               .title      = "Histogram",
               .get_pos    = histogram_view_get_pos,
               .set_pos    = histogram_view_set_pos,
               .get_size   = histogram_get_size_wrap,
               .is_visible = histogram_view_is_enabled,
               .on_toggle  = histogram_view_toggle_enabled  },
        [1] = {.id         = "vectorscope",
               .title      = "Vectorscope",
               .get_pos    = vectorscope_view_get_pos,
               .set_pos    = vectorscope_view_set_pos,
               .get_size   = vectorscope_get_size_wrap,
               .is_visible = vectorscope_view_is_enabled,
               .on_toggle  = vectorscope_view_toggle_enabled},
        [2] = {.id         = "stats",
               .title      = "Stats",
               .get_pos    = stats_overlay_get_pos,
               .set_pos    = stats_overlay_set_pos,
               .get_size   = stats_overlay_get_size,
               .is_visible = analytics_is_stats_visible,
               .on_toggle  = analytics_toggle_stats_visible },
        [3] = {.id         = "flicker",
               .title      = "Flicker",
               .get_pos    = flicker_view_get_pos,
               .set_pos    = flicker_view_set_pos,
               .get_size   = flicker_get_size_wrap,
               .is_visible = flicker_view_is_enabled,
               .on_toggle  = flicker_view_toggle_enabled    },
    };
    for (size_t i = 0; i < sizeof(panels) / sizeof(panels[ 0 ]); i++)
    {
        panel_register(&panels[ i ]);
    }

    return true;
}

//------------------------------------------------------- Cleanup -----------------------------------------------------

void renderer_cleanup(void)
{
    panel_unregister_all();

    screenshot_cleanup();
    flicker_view_cleanup();
    vectorscope_view_cleanup();
    histogram_view_cleanup();
    post_pass_cleanup();
    format_pass_cleanup();
    diff_view_cleanup();
    flicker_reset();
    frame_context_clear();

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

//------------------------------------------------------- Window / Polling ---------------------------------------------

GLFWwindow * renderer_get_window(void)
{
    return window_get();
}

void renderer_poll_events(void)
{
    window_poll_events();
}

//------------------------------------------------------- Frame --------------------------------------------------------

void renderer_set_frame(const uint8_t * data)
{
    g_last_frame = data;

    const uint8_t * frame_to_upload = diff_view_process(data, g_win, g_format.bytes_per_pixel, g_eff_bits);
    format_pass_upload(frame_to_upload);

    renderer_refresh_analytics();
}

//------------------------------------------------------- Draw ---------------------------------------------------------

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

    // GL-контент панелей (до overlay)
    panel_clamp_to_window(win, g_dpi_scale);
    histogram_view_render(histogram_view_get_pos(), win, g_dpi_scale);
    vectorscope_view_render(vectorscope_view_get_pos(), win, g_dpi_scale);
    flicker_view_render(flicker_view_get_pos(), win, g_dpi_scale);

    // Overlay
    overlay_begin(win);
    panel_draw_chrome(win, g_dpi_scale);
    selection_view_draw(&g_selection, viewport_get_point(), viewport_get_size(), g_win);
    pixel_probe_draw(win, g_dpi_scale);
    defect_view_draw(win, g_dpi_scale);
    stats_overlay_draw(stats_overlay_get_pos(), g_dpi_scale);
    flicker_view_draw_text(flicker_view_get_pos(), g_dpi_scale);
    line_profile_view_draw(win, g_dpi_scale);
    progress_bar_draw(win, g_dpi_scale);
    overlay_end();

    // Контекстное меню — автономно (свой overlay + NanoVG)
    context_menu_draw(win, g_dpi_scale);

    screenshot_process();
    window_swap_buffers();
}

//------------------------------------------------------- Simple accessors ---------------------------------------------

int renderer_get_bytes_per_pixel(void)
{
    return format_pass_get_bytes_per_pixel();
}

float renderer_get_dpi_scale(void)
{
    return g_dpi_scale;
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

void renderer_request_screenshot(void)
{
    screenshot_request();
}

//------------------------------------------------------- Smoothing (управление) --------------------------------------

void renderer_toggle_smoothing(void)
{
    g_smoothing = !g_smoothing;
    apply_smoothing();
    window_title_refresh();
    settings_set_feature_bool("smoothing", "enabled", g_smoothing);
    log_info("Smoothing %s", g_smoothing ? "enabled" : "disabled");
}

bool renderer_is_smoothing_active(void)
{
    return g_smoothing;
}

//------------------------------------------------------- Analytics refresh --------------------------------------------

void renderer_refresh_analytics(void)
{
    if (!g_last_frame)
    {
        return;
    }

    frame_context_set(g_last_frame, g_win, &g_format, g_eff_bits);

    analytics_update(g_last_frame, g_win, &g_format, &g_selection, g_eff_bits);
    flicker_update(&g_selection);

    histogram_view_update(g_eff_bits);
    vectorscope_view_update();
    flicker_view_update();

    if (post_pass_is_gradient_active())
    {
        g_sharpness = sharpness_compute(g_last_frame, g_win, &g_format, &g_selection);
        window_title_set_sharpness(g_sharpness.valid, g_sharpness.lap_var, g_sharpness.tenengrad);
    }
}

//------------------------------------------------------- Selection ----------------------------------------------------

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

void renderer_clear_selection(void)
{
    selection_clear(&g_selection);
}

bool renderer_screen_to_frame(point_s screen, point_s * frame)
{
    return viewport_screen_to_frame(screen, frame);
}

//------------------------------------------------------- Zoom / Pan ---------------------------------------------------

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

//------------------------------------------------------- Mouse / Menu ------------------------------------------------

void renderer_set_mouse_pos(point_s mouse)
{
    pixel_probe_set_mouse(mouse);
    progress_bar_notify_mouse(mouse);
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
