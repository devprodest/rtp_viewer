#include "viewport.h"

#include "util/utils.h"
#include "window.h"

#define ZOOM_MIN 1.0f
#define ZOOM_MAX 32.0f

static image_size_s g_frame = {};
static point_s g_point      = {};
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
    g_point      = (point_s) {0};
    g_size       = (image_size_s) {0};
}

void viewport_update(image_size_s win)
{
    point_s base_start = {0};
    image_size_s base  = {0};
    compute_base(win, &base_start, &base);

    g_point.x = base_start.x - (int)(g_pan.x * g_zoom);
    g_point.y = base_start.y - (int)(g_pan.y * g_zoom);
    g_size.x  = (int)((float)base.x * g_zoom);
    g_size.y  = (int)((float)base.y * g_zoom);
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
}

void viewport_apply_wheel(double wheel, point_s screen)
{
    if (wheel == 0.0 || g_size.x <= 0 || g_size.y <= 0)
    {
        return;
    }

    point_s frame = {0};
    if (!viewport_screen_to_frame(screen, &frame))
    {
        return;
    }

    float factor = utils_max(1.0f + (float)wheel * 0.15f, 0.1f);
    g_zoom       = utils_clamp(g_zoom * factor, ZOOM_MIN, ZOOM_MAX);

    image_size_s win = window_get_framebuffer_size();
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }

    point_s base_start = {0};
    image_size_s base  = {0};
    compute_base(win, &base_start, &base);

    float want_x = (float)screen.x - (float)base_start.x - ((float)frame.x * (float)base.x * g_zoom) / (float)g_frame.x;
    float want_y = (float)screen.y - (float)base_start.y - ((float)frame.y * (float)base.y * g_zoom) / (float)g_frame.y;

    g_pan.x = -want_x / g_zoom;
    g_pan.y = -want_y / g_zoom;
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
    }
}

bool viewport_screen_to_frame(point_s screen, point_s * frame)
{
    if (g_size.x <= 0 || g_size.y <= 0)
    {
        return false;
    }

    if (screen.x < g_point.x || screen.x >= g_point.x + g_size.x || //
        screen.y < g_point.y || screen.y >= g_point.y + g_size.y)
    {
        return false;
    }

    int frame_x = (screen.x - g_point.x) * g_frame.x / g_size.x;
    int frame_y = (screen.y - g_point.y) * g_frame.y / g_size.y;

    frame->x = utils_clamp(frame_x, 0, g_frame.x - 1);
    frame->y = utils_clamp(frame_y, 0, g_frame.y - 1);

    return true;
}
