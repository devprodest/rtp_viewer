#include "line_profile_view.h"
#include "render/overlay.h"
#include "render/ui/pixel_probe.h"
#include "render/ui/ui_color_scheme.h"
#include "render/viewport.h"
#include "util/utils.h"

#define PANEL_THICKNESS 96

static bool g_enabled = false;

void line_profile_view_toggle(void)
{
    g_enabled = !g_enabled;
}
bool line_profile_view_is_enabled(void)
{
    return g_enabled;
}

void line_profile_view_draw(image_size_s win, float dpi_scale)
{
    (void)dpi_scale;
    if (!g_enabled)
    {
        return;
    }
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }

    // Получаем позицию курсора и её координаты в кадре
    point_s mouse = pixel_probe_get_mouse();
    point_s cursor_frame;
    if (!viewport_screen_to_frame(mouse, &cursor_frame))
    {
        return;
    }

    const image_size_s fsize = pixel_probe_get_frame_size();
    if (cursor_frame.x < 0 || cursor_frame.x >= fsize.x)
    {
        return;
    }
    if (cursor_frame.y < 0 || cursor_frame.y >= fsize.y)
    {
        return;
    }

    const int thickness = PANEL_THICKNESS; // в пикселях framebuffer
    const int bpp       = pixel_probe_get_bpp();
    const float max_val = (bpp == 2) ? 65535.0f : 255.0f;

    //----------------------------------------------------------
    // Левая панель: вертикальный профиль (срез по столбцу)
    //----------------------------------------------------------
    overlay_rect_draw((rect_s) {0, 0, thickness, win.y}, UI_LINE_PROFILE_BG);

    for (int sy = 0; sy < win.y; sy++)
    {
        point_s f;
        if (!viewport_screen_to_frame((point_s) {mouse.x, sy}, &f))
        {
            continue;
        }
        if (f.y < 0 || f.y >= fsize.y)
        {
            continue;
        }

        pixel_sample_s ps = pixel_probe_read_pixel((point_s) {cursor_frame.x, f.y});
        float val         = (bpp <= 2) ? (float)ps.raw : (0.299f * ps.rgb.r + 0.587f * ps.rgb.g + 0.114f * ps.rgb.b);
        float norm        = utils_clamp(val / max_val, 0.0f, 1.0f);

        int bar_w = (int)(norm * thickness);
        if (bar_w > 0)
        {
            overlay_rect_draw((rect_s) {0, sy, bar_w, sy + 1}, UI_LINE_PROFILE_Y);
        }
    }

    //----------------------------------------------------------
    // Нижняя панель: горизонтальный профиль (срез по строке)
    //----------------------------------------------------------
    overlay_rect_draw((rect_s) {0, win.y - thickness, win.x, win.y}, UI_LINE_PROFILE_BG);

    for (int sx = 0; sx < win.x; sx++)
    {
        point_s f;
        if (!viewport_screen_to_frame((point_s) {sx, mouse.y}, &f))
        {
            continue;
        }
        if (f.x < 0 || f.x >= fsize.x)
        {
            continue;
        }

        pixel_sample_s ps = pixel_probe_read_pixel((point_s) {f.x, cursor_frame.y});
        float val         = (bpp <= 2) ? (float)ps.raw : (0.299f * ps.rgb.r + 0.587f * ps.rgb.g + 0.114f * ps.rgb.b);
        float norm        = utils_clamp(val / max_val, 0.0f, 1.0f);

        int bar_h = (int)(norm * thickness);
        if (bar_h > 0)
        {
            overlay_rect_draw((rect_s) {sx, win.y - bar_h, sx + 1, win.y}, UI_LINE_PROFILE_X);
        }
    }
}
