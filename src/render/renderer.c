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
#include "post_pass.h"
#include "render/selection_view.h"
#include "save/saver.h"
#include "save/screenshot.h"
#include "overlay.h"
#include "window.h"

#include <stdio.h>
#include <string.h>

//----------------------------------------------------------------------------------------------------------------------

#define ZOOM_MIN 1.0f
#define ZOOM_MAX 32.0f

//----------------------------------------------------------------------------------------------------------------------

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

static float g_dpi_scale = 1.0f;


static float g_zoom        = 1.0f;
static float g_pan_x       = 0.0f;
static float g_pan_y       = 0.0f;
static bool g_pan_drag     = false;
static double g_pan_last_x = 0.0;
static double g_pan_last_y = 0.0;

static bool g_left_was_down  = false;
static bool g_right_was_down = false;

static float g_inspector_text_scale = 1.5f;
static bool g_inspector_enabled     = false;
static int g_last_mouse_x           = 0;
static int g_last_mouse_y           = 0;
static const uint8_t * g_last_frame = nullptr;

static bool g_gradient_enabled = false;
static sharpness_s g_sharpness;

//----------------------------------------------------------------------------------------------------------------------

static void compute_base_viewport(int win_w, int win_h, int * bx, int * by, int * bw, int * bh);

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

void renderer_refresh_title(void)
{
    if (!window_get())
    {
        return;
    }

    char title[ 160 ];

    char extra[ 64 ] = "";
    if (g_gradient_enabled && g_sharpness.valid)
    {
        snprintf(extra, sizeof(extra), " Lap=%.1f Ten=%.0f", g_sharpness.lap_var, g_sharpness.tenengrad);
    }

    snprintf(title, sizeof(title), "RTP Viewer - %dx%d %s fps:%.1f%s%s%s", //
             g_width, g_height,                                            //
             g_format_name[ 0 ] ? g_format_name : "?",                     //
             stats_get_fps(),                                              //
             g_paused ? " [PAUSE]" : "",                                   //
             saver_is_recording() ? " [REC]" : "",                         //
             extra);

    window_set_title(title);
}

bool renderer_init(int width, int height, const char * format_name, int effective_bits, bool smooth_start)
{
    if (!window_init(width, height, "RTP Viewer"))
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
    g_left_was_down  = false;
    g_right_was_down = false;

    overlay_init();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    g_width    = width;
    g_height   = height;
    g_eff_bits = effective_bits;
    snprintf(g_format_name, sizeof(g_format_name), "%s", format_name);

    g_quad_vao = gl_utils_create_fullscreen_quad();
    if (g_quad_vao == 0)
    {
        log_error("renderer: failed to create fullscreen quad VAO\n");
        overlay_cleanup();
        window_cleanup();
        return false;
    }

    if (!format_pass_init(width, height, g_quad_vao, effective_bits, &g_format))
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

    post_pass_set_texel_size(1.0f / (float)g_width, 1.0f / (float)g_height);

    if (!histogram_view_init(g_quad_vao))
    {
        post_pass_cleanup();
        format_pass_cleanup();
        overlay_cleanup();
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

    overlay_cleanup();
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
    g_last_frame = data;

    format_pass_upload(data);

    analytics_update(data, g_width, g_height, &g_format, g_format_name, &g_selection);

    if (g_gradient_enabled)
    {
        sharpness_compute(data, g_width, g_height, &g_format, &g_selection, &g_sharpness);
    }

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

    // блок расчёта viewport
    // {
    int base_x = 0;
    int base_y = 0;
    int base_w = 0;
    int base_h = 0;
    compute_base_viewport(win_w, win_h, &base_x, &base_y, &base_w, &base_h);

    int vp_w = base_w * g_zoom;
    int vp_h = base_h * g_zoom;
    int vp_x = base_x - (g_pan_x * g_zoom);
    int vp_y = base_y - (g_pan_y * g_zoom);
    // }

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

    everlay_begin((image_size_s) {win_w, win_h});
    selection_view_draw(&g_selection, g_vp_x, g_vp_y, g_vp_w, g_vp_h, g_width, g_height);
    if (g_inspector_enabled)
    {
        renderer_draw_inspector(g_last_mouse_x, g_last_mouse_y);
    }
    everlay_end();

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

void renderer_zoom_reset(void)
{
    g_zoom  = 1.0f;
    g_pan_x = 0.0f;
    g_pan_y = 0.0f;
}

static void compute_base_viewport(int win_w, int win_h, int * bx, int * by, int * bw, int * bh)
{
    float aspect_img = (float)g_width / (float)g_height;
    float aspect_win = (float)win_w / (float)win_h;

    if (aspect_win > aspect_img)
    {
        *bh = win_h;
        *bw = (int)(win_h * aspect_img);
        *bx = (win_w - *bw) / 2;
        *by = 0;
    }
    else
    {
        *bw = win_w;
        *bh = (int)(win_w / aspect_img);
        *bx = 0;
        *by = (win_h - *bh) / 2;
    }
}

void renderer_zoom_apply_wheel(double wheel, double mx, double my)
{
    if (wheel == 0.0 || g_vp_w <= 0 || g_vp_h <= 0)
    {
        return;
    }

    // Точка под курсором в координатах кадра до zoom
    int fx_before = 0;
    int fy_before = 0;
    if (!renderer_screen_to_frame(mx, my, &fx_before, &fy_before))
    {
        return;
    }

    float factor = 1.0f + (float)wheel * 0.15f;
    if (factor < 0.1f)
    {
        factor = 0.1f;
    }

    float new_zoom = g_zoom * factor;
    if (new_zoom < ZOOM_MIN)
    {
        new_zoom = ZOOM_MIN;
    }
    if (new_zoom > ZOOM_MAX)
    {
        new_zoom = ZOOM_MAX;
    }

    g_zoom = new_zoom;

    // После zoom та же точка должна остаться под курсором.
    // Пересчитываем pan так, чтобы frame (fx_before, fy_before) проецировался в (mx, my).
    // Проекция: x_screen = vp_x + (f_x * vp_w) / W
    //          = base_x - pan_x*zoom + (f_x * base_w * zoom) / W
    // Отсюда: pan_x = (base_x + (f_x * base_w * zoom) / W - mx) / zoom
    // base_x и base_w — исходный letterbox без zoom.

    // Проще пересчитать через текущий g_vp_* и новую позицию:
    // Не храним base, пересчитаем по текущему состоянию.
    // Мы только что поменяли g_zoom, но g_vp_* ещё старые. Пересчёт сделаем
    // на следующем кадре в update_texture, а pan подберём аналитически.

    // Найдём base_w/base_h/base_x/base_y из win размеров — они уже в g_vp_*, но с учётом старого zoom.
    // Проще: пересчитать base прямо здесь.
    int win_w = 0;
    int win_h = 0;
    window_get_framebuffer_size(&win_w, &win_h);
    if (win_w <= 0 || win_h <= 0)
    {
        return;
    }

    int base_w = 0;
    int base_h = 0;
    int base_x = 0;
    int base_y = 0;
    compute_base_viewport(win_w, win_h, &base_x, &base_y, &base_w, &base_h);


    float want_x = (float)mx - (float)base_x - ((float)fx_before * (float)base_w * g_zoom) / (float)g_width;
    float want_y = (float)my - (float)base_y - ((float)fy_before * (float)base_h * g_zoom) / (float)g_height;

    g_pan_x = -want_x / g_zoom;
    g_pan_y = -want_y / g_zoom;
}

void renderer_pan_apply_drag(double mx, double my, bool middle_down)
{
    if (middle_down && !g_pan_drag)
    {
        g_pan_drag   = true;
        g_pan_last_x = mx;
        g_pan_last_y = my;
        return;
    }

    if (!middle_down && g_pan_drag)
    {
        g_pan_drag = false;
        return;
    }

    if (middle_down && g_pan_drag)
    {
        double dx    = mx - g_pan_last_x;
        double dy    = my - g_pan_last_y;
        g_pan_last_x = mx;
        g_pan_last_y = my;

        g_pan_x -= dx / g_zoom;
        g_pan_y += dy / g_zoom;
    }
}

void renderer_draw_inspector(int mouse_x, int mouse_y)
{
    int fx = 0;
    int fy = 0;
    if (!renderer_screen_to_frame((double)mouse_x, (double)mouse_y, &fx, &fy))
    {
        return;
    }

    if (!g_last_frame)
    {
        return;
    }

    uint8_t r    = 0;
    uint8_t g    = 0;
    uint8_t b    = 0;
    uint32_t raw = 0;

    if (g_format.bytes_per_pixel == 1)
    {
        uint8_t v = g_last_frame[ (size_t)fy * (size_t)g_width + (size_t)fx ];
        r = g = b = v;
        raw       = v;
    }
    else if (g_format.bytes_per_pixel == 2)
    {
        const uint16_t * p = (const uint16_t *)g_last_frame;
        uint16_t v         = p[ (size_t)fy * (size_t)g_width + (size_t)fx ];
        raw                = v;
        uint8_t v8         = (uint8_t)(v >> 8);
        r = g = b = v8;
    }
    else if (g_format.bytes_per_pixel == 3)
    {
        size_t idx = ((size_t)fy * (size_t)g_width + (size_t)fx) * 3;
        r          = g_last_frame[ idx + 0 ];
        g          = g_last_frame[ idx + 1 ];
        b          = g_last_frame[ idx + 2 ];
    }
    else if (g_format.bytes_per_pixel == 4)
    {
        size_t idx = ((size_t)fy * (size_t)g_width + (size_t)fx) * 4;
        r          = g_last_frame[ idx + 0 ];
        g          = g_last_frame[ idx + 1 ];
        b          = g_last_frame[ idx + 2 ];
    }

    int win_w = 0;
    int win_h = 0;
    window_get_framebuffer_size(&win_w, &win_h);

    const float text_scale = g_dpi_scale * g_inspector_text_scale;
    const int margin       = (int)(10 * g_dpi_scale);
    const int box_size     = (int)(40 * g_dpi_scale);
    const int gap          = (int)(10 * g_dpi_scale);
    const int pad          = (int)(6 * g_dpi_scale);
    const int line_h       = (int)(14 * text_scale);

    const int box_x  = margin;
    const int box_y  = win_h - margin - box_size;
    const int text_x = box_x + box_size + gap;


    typedef char text_line_t[ 64 ];
    text_line_t line[ 2 ];

    snprintf(line[ 0 ], sizeof(text_line_t), "x;y=(%d;%d)", fx, fy);

    if (g_format.bytes_per_pixel <= 2)
    {
        snprintf(line[ 1 ], sizeof(text_line_t), "val=%u", raw);
    }
    else
    {
        snprintf(line[ 1 ], sizeof(text_line_t), "RGB=(%u;%u;%u)", r, g, b);
    }

    int w1     = text_measure(line[ 0 ], text_scale);
    int w2     = text_measure(line[ 1 ], text_scale);
    int text_w = (w1 > w2) ? w1 : w2;

    // Плашка под текстом, справа от квадрата

    rect_s panel = {
        .x0 = text_x - pad,
        .y0 = box_y - pad,
        .x1 = text_x + text_w + pad,
        .y1 = box_y + line_h + line_h + pad,
    };
    overlay_rect(panel, (fcolor_rgba_s) {0.0f, 0.0f, 0.0f, 0.6f});

    // Квадрат цвета пикселя + рамка
    rect_s pix_color = {box_x, box_y, box_x + box_size, box_y + box_size};

    overlay_rect(pix_color, (fcolor_rgba_s) {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f});
    overlay_rect_outline(pix_color, (fcolor_rgba_s) {1.0f, 1.0f, 1.0f, 1.0f}, 1);

    // Текст
    for (size_t i = 0; i < 2; i++)
    {
        text_draw((point_s) {text_x, box_y + i * line_h}, line[ i ], (fcolor_rgb_s) {1.0f, 1.0f, 1.0f}, text_scale);
    }
}

void renderer_toggle_inspector(void)
{
    g_inspector_enabled = !g_inspector_enabled;
    log_info("Inspector %s", g_inspector_enabled ? "enabled" : "disabled");
}

void renderer_set_mouse_pos(int mx, int my)
{
    g_last_mouse_x = mx;
    g_last_mouse_y = my;
}

void renderer_toggle_gradient(void)
{
    g_gradient_enabled = !g_gradient_enabled;
    post_pass_set_gradient(g_gradient_enabled);
    renderer_refresh_title();
    log_info("Gradient %s", g_gradient_enabled ? "enabled" : "disabled");
}
