#include "viewport.h"
#include "util/utils.h"
#include "window.h"

#define ZOOM_MIN 1.0f
#define ZOOM_MAX 32.0f

static image_size_s g_frame = {};
static fpoint_s g_point_f   = {}; // рабочее значение (float, без потерь)
static fpoint_s g_size_f    = {};
static point_s g_point      = {}; // int-копии для API (post_pass, selection_view)
static image_size_s g_size  = {};

static float g_zoom        = 1.0f;
static fpoint_s g_pan      = {};
static fpoint_s g_pan_last = {};
static bool g_pan_drag     = false;


/// \brief Считает базовый letterbox-вьюпорт без zoom/pan.
static void compute_base(image_size_s win, point_s * base_start, image_size_s * base_size)
{
    float aspect_img = (float)g_frame.x / (float)g_frame.y;
    float aspect_win = (float)win.x / (float)win.y;

    if (aspect_win > aspect_img)
    {
        base_size->y  = win.y;
        base_size->x  = (int)((float)win.y * aspect_img);
        base_start->x = (win.x - base_size->x) / 2;
        base_start->y = 0;
    }
    else
    {
        base_size->x  = win.x;
        base_size->y  = (int)((float)win.x / aspect_img);
        base_start->x = 0;
        base_start->y = (win.y - base_size->y) / 2;
    }
}

void viewport_init(image_size_s frame_size)
{
    g_frame      = frame_size;
    g_zoom       = 1.0f;
    g_pan.x      = 0.0f;
    g_pan.y      = 0.0f;
    g_pan_last.x = 0.0f;
    g_pan_last.y = 0.0f;
    g_pan_drag   = false;
    g_point_f    = (fpoint_s) {0};
    g_size_f     = (fpoint_s) {0};
    g_point      = (point_s) {0};
    g_size       = (image_size_s) {0};
}

void viewport_update(image_size_s win)
{
    point_s base_start = {0};
    image_size_s base  = {0};
    compute_base(win, &base_start, &base);

    g_point_f.x = (float)base_start.x - g_pan.x * g_zoom;
    g_point_f.y = (float)base_start.y - g_pan.y * g_zoom;
    g_size_f.x  = (float)base.x * g_zoom;
    g_size_f.y  = (float)base.y * g_zoom;

    g_point.x = (int)(g_point_f.x + 0.5f);
    g_point.y = (int)(g_point_f.y + 0.5f);
    g_size.x  = (int)(g_size_f.x + 0.5f);
    g_size.y  = (int)(g_size_f.y + 0.5f);
}

point_s viewport_get_point(void)
{
    return g_point;
}

image_size_s viewport_get_size(void)
{
    return g_size;
}

void viewport_reset(void)
{
    g_zoom  = 1.0f;
    g_pan.x = 0.0f;
    g_pan.y = 0.0f;

    image_size_s win = window_get_framebuffer_size();
    if (win.x > 0 && win.y > 0)
    {
        viewport_update(win);
    }
}

void viewport_apply_wheel(double wheel, point_s screen)
{
    if (wheel == 0.0)
    {
        return;
    }
    if (g_frame.x <= 0 || g_frame.y <= 0)
    {
        return;
    }
    if (g_size_f.x <= 0.0f || g_size_f.y <= 0.0f)
    {
        return;
    }

    image_size_s win = window_get_framebuffer_size();
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }

    // Курсор должен быть в текущей видимой области
    if ((float)screen.x < g_point_f.x || (float)screen.x >= g_point_f.x + g_size_f.x || (float)screen.y < g_point_f.y ||
        (float)screen.y >= g_point_f.y + g_size_f.y)
    {
        return;
    }

    // Sub-pixel позиция кадра под курсором. int-версия даёт сдвиг до
    // 0.5 * size / frame_w = ~10-16 px экрана на клик при zoom >= 10.
    const float fframe_x = ((float)screen.x - g_point_f.x) * (float)g_frame.x / g_size_f.x;
    const float fframe_y = ((float)screen.y - g_point_f.y) * (float)g_frame.y / g_size_f.y;

    float factor = utils_max(1.0f + (float)wheel * 0.15f, 0.1f);
    g_zoom       = utils_clamp(g_zoom * factor, ZOOM_MIN, ZOOM_MAX);

    point_s base_start = {0};
    image_size_s base  = {0};
    compute_base(win, &base_start, &base);

    // Anchor в double: держим один и тот же sub-pixel кадра под курсором.
    const double new_size_x   = (double)base.x * (double)g_zoom;
    const double new_size_y   = (double)base.y * (double)g_zoom;
    const double want_point_x = (double)screen.x - (double)fframe_x * new_size_x / (double)g_frame.x;
    const double want_point_y = (double)screen.y - (double)fframe_y * new_size_y / (double)g_frame.y;

    g_pan.x = (float)(((double)base_start.x - want_point_x) / (double)g_zoom);
    g_pan.y = (float)(((double)base_start.y - want_point_y) / (double)g_zoom);

    viewport_update(win);
}

void viewport_apply_pan_drag(point_s screen, bool middle_down)
{
    if (middle_down && !g_pan_drag)
    {
        g_pan_drag   = true;
        g_pan_last.x = screen.x;
        g_pan_last.y = screen.y;
        return;
    }

    if (!middle_down && g_pan_drag)
    {
        g_pan_drag = false;
        return;
    }

    if (middle_down && g_pan_drag)
    {
        float dx     = screen.x - g_pan_last.x;
        float dy     = screen.y - g_pan_last.y;
        g_pan_last.x = screen.x;
        g_pan_last.y = screen.y;

        g_pan.x -= dx / g_zoom;
        g_pan.y -= dy / g_zoom;

        image_size_s win = window_get_framebuffer_size();
        if (win.x > 0 && win.y > 0)
        {
            viewport_update(win);
        }
    }
}

bool viewport_screen_to_frame(point_s screen, point_s * frame)
{
    if (g_size_f.x <= 0.0f || g_size_f.y <= 0.0f)
    {
        return false;
    }

    if ((float)screen.x < g_point_f.x || (float)screen.x >= g_point_f.x + g_size_f.x || //
        (float)screen.y < g_point_f.y || (float)screen.y >= g_point_f.y + g_size_f.y)
    {
        return false;
    }

    float fx = ((float)screen.x - g_point_f.x) * (float)g_frame.x / g_size_f.x;
    float fy = ((float)screen.y - g_point_f.y) * (float)g_frame.y / g_size_f.y;

    frame->x = utils_clamp((int)fx, 0, g_frame.x - 1);
    frame->y = utils_clamp((int)fy, 0, g_frame.y - 1);

    return true;
}

bool viewport_frame_to_screen(point_s frame, point_s * screen)
{
    if (g_size_f.x <= 0.0f || g_size_f.y <= 0.0f)
    {
        return false;
    }
    if (frame.x < 0 || frame.x >= g_frame.x)
    {
        return false;
    }
    if (frame.y < 0 || frame.y >= g_frame.y)
    {
        return false;
    }

    screen->x = (int)(g_point_f.x + (float)frame.x * g_size_f.x / (float)g_frame.x);
    screen->y = (int)(g_point_f.y + (float)frame.y * g_size_f.y / (float)g_frame.y);
    return true;
}
