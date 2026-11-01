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

static bool g_saturation_enabled = false;

static format_config_s g_format;
static selection_s g_selection;

static float g_dpi_scale = 1.0f;

static bool g_left_was_down = false;

static bool g_inspector_enabled     = false;
static float g_inspector_text_scale = 1.5f;
static point_s g_last_mouse         = {};
static const uint8_t * g_last_frame = nullptr;

static bool g_gradient_enabled = false;
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
    post_pass_cleanup();
    format_pass_cleanup();

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
    g_last_frame = data;

    format_pass_upload(data);

    analytics_update(data, g_win, &g_format, &g_selection, g_eff_bits);

    if (g_gradient_enabled)
    {
        g_sharpness = sharpness_compute(data, g_win, &g_format, &g_selection);
        window_title_set_sharpness(g_sharpness.valid, g_sharpness.lap_var, g_sharpness.tenengrad);
    }

    if (histogram_view_is_enabled())
    {
        histogram_view_update();
    }

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

    if (histogram_view_is_enabled() && histogram_view_get_active_channels() > 0)
    {
        const image_size_s hist_view_size = {320, 240};

        const int margin = 10;
        const point_s ov = {
            .x = utils_max(win.x - hist_view_size.x - margin, 0),
            .y = utils_max(win.y - hist_view_size.y - margin, 0),
        };
        histogram_view_render(ov, hist_view_size);
    }

    overlay_begin(win);
    selection_view_draw(&g_selection, viewport_get_point(), viewport_get_size(), g_win);
    if (g_inspector_enabled)
    {
        renderer_draw_inspector(g_last_mouse);
    }
    overlay_end();

    overlay_begin(win);
    selection_view_draw(&g_selection, viewport_get_point(), viewport_get_size(), g_win);
    if (g_inspector_enabled)
    {
        renderer_draw_inspector(g_last_mouse);
    }

    context_menu_draw(win, g_dpi_scale); // <-- добавить

    overlay_end();

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

void renderer_toggle_histogram_cdf(void)
{
    histogram_view_toggle_cdf();
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

void renderer_set_selection_from_mouse(point_s mouse, bool left_down, bool in_view)
{
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

void renderer_draw_inspector(point_s point)
{
    point_s frame;
    if (!viewport_screen_to_frame(point, &frame))
    {
        return;
    }

    if (!g_last_frame)
    {
        return;
    }

    color_rgb_s rgb = {0, 0, 0};
    uint32_t raw    = 0;

    if (g_format.bytes_per_pixel == 1)
    {
        uint8_t v = g_last_frame[ ((size_t)frame.y * (size_t)g_win.x) + (size_t)frame.x ];
        rgb       = (color_rgb_s) {v, v, v};
        raw       = v;
    }
    else if (g_format.bytes_per_pixel == 2)
    {
        const uint16_t * p = (const uint16_t *)g_last_frame;
        uint16_t v         = p[ ((size_t)frame.y * (size_t)g_win.x) + (size_t)frame.x ];
        raw                = v;
        uint8_t v8         = (uint8_t)(v >> 8);
        rgb                = (color_rgb_s) {v8, v8, v8};
    }
    else if (g_format.bytes_per_pixel == 3)
    {
        size_t idx = (((size_t)frame.y * (size_t)g_win.x) + (size_t)frame.x) * 3;
        rgb.r      = g_last_frame[ idx + 0 ];
        rgb.g      = g_last_frame[ idx + 1 ];
        rgb.b      = g_last_frame[ idx + 2 ];
    }
    else if (g_format.bytes_per_pixel == 4)
    {
        size_t idx = (((size_t)frame.y * (size_t)g_win.x) + (size_t)frame.x) * 4;
        rgb.r      = g_last_frame[ idx + 0 ];
        rgb.g      = g_last_frame[ idx + 1 ];
        rgb.b      = g_last_frame[ idx + 2 ];
    }

    image_size_s win = window_get_framebuffer_size();

    const float text_scale = g_dpi_scale * g_inspector_text_scale;
    const int margin       = (int)(10 * g_dpi_scale);
    const int box_size     = (int)(40 * g_dpi_scale);
    const int gap          = (int)(10 * g_dpi_scale);
    const int pad          = (int)(6 * g_dpi_scale);
    const int line_h       = (int)(14 * text_scale);

    const int box_x  = margin;
    const int box_y  = win.y - margin - box_size;
    const int text_x = box_x + box_size + gap;


    typedef char text_line_t[ 64 ];
    text_line_t line[ 2 ];

    snprintf(line[ 0 ], sizeof(text_line_t), "x;y=(%d;%d)", frame.x, frame.y);

    if (g_format.bytes_per_pixel <= 2)
    {
        snprintf(line[ 1 ], sizeof(text_line_t), "val=%u", raw);
    }
    else
    {
        snprintf(line[ 1 ], sizeof(text_line_t), "RGB=(%u;%u;%u)", rgb.r, rgb.g, rgb.b);
    }

    int w1     = overlay_text_measure(line[ 0 ], text_scale);
    int w2     = overlay_text_measure(line[ 1 ], text_scale);
    int text_w = (w1 > w2) ? w1 : w2;

    // Плашка под текстом, справа от квадрата

    rect_s panel = {
        .x0 = text_x - pad,
        .y0 = box_y - pad,
        .x1 = text_x + text_w + pad,
        .y1 = box_y + line_h + line_h + pad,
    };
    overlay_rect_draw(panel, (fcolor_rgba_s) {0.0f, 0.0f, 0.0f, 0.6f});

    // Квадрат цвета пикселя + рамка
    rect_s pix_color = {box_x, box_y, box_x + box_size, box_y + box_size};

    overlay_rect_draw(pix_color, (fcolor_rgba_s) {rgb.r / 255.0f, rgb.g / 255.0f, rgb.b / 255.0f, 1.0f});
    overlay_rect_outline_draw(pix_color, (fcolor_rgba_s) {1.0f, 1.0f, 1.0f, 1.0f}, 1);

    // Текст
    for (size_t i = 0; i < 2; i++)
    {
        overlay_text_draw((point_s) {text_x, box_y + (i * line_h)}, line[ i ], (fcolor_rgb_s) {1.0f, 1.0f, 1.0f},
                          text_scale);
    }
}

void renderer_toggle_inspector(void)
{
    g_inspector_enabled = !g_inspector_enabled;
    log_info("Inspector %s", g_inspector_enabled ? "enabled" : "disabled");
}

void renderer_set_mouse_pos(point_s mouse)
{
    g_last_mouse = mouse;
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
bool renderer_is_inspector_active(void)
{
    return g_inspector_enabled;
}
