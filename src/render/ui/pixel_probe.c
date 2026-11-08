#include "pixel_probe.h"

#include "analysis/frame_context.h"
#include "render/overlay.h"
#include "render/ui/ui_color_scheme.h"
#include "render/viewport.h"
#include "util/utils.h"

#include <stdio.h>

static bool g_enabled  = false;
static point_s g_mouse = {-1, -1};

void pixel_probe_toggle(void)
{
    g_enabled = !g_enabled;
}
bool pixel_probe_is_enabled(void)
{
    return g_enabled;
}
void pixel_probe_set_mouse(point_s screen)
{
    g_mouse = screen;
}
point_s pixel_probe_get_mouse(void)
{
    return g_mouse;
}

void pixel_probe_draw(image_size_s win, float dpi_scale)
{
    if (!g_enabled || !frame_context_is_valid())
    {
        return;
    }
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }

    const int mx = g_mouse.x;
    const int my = g_mouse.y;
    if (mx < 0 || my < 0 || mx >= win.x || my >= win.y)
    {
        return;
    }

    point_s frame;
    if (!viewport_screen_to_frame(g_mouse, &frame))
    {
        return;
    }

    image_size_s fsize = frame_context_size();
    if (frame.x < 0 || frame.x >= fsize.x)
    {
        return;
    }
    if (frame.y < 0 || frame.y >= fsize.y)
    {
        return;
    }

    // Перекрестие
    int lw = utils_max(1, (int)(1 * dpi_scale));
    overlay_rect_draw((rect_s) {0, my, win.x, my + lw}, UI_CROSSHAIR_LINE);
    overlay_rect_draw((rect_s) {mx, 0, mx + lw, win.y}, UI_CROSSHAIR_LINE);

    // Пиксель
    frame_pixel_t px = frame_context_read(frame.x, frame.y);

    char line1[ 64 ];
    char line2[ 64 ];
    snprintf(line1, sizeof(line1), "x;y=(%d;%d)", frame.x, frame.y);

    int bpp = frame_context_bpp();
    if (bpp <= 2)
    {
        snprintf(line2, sizeof(line2), "val=%u", px.raw);
    }
    else
    {
        snprintf(line2, sizeof(line2), "RGB=(%u;%u;%u)", px.r, px.g, px.b);
    }

    // Геометрия панели
    const float text_scale = dpi_scale * 1.2f;
    const int pad          = 6 * dpi_scale;
    const int box_size     = 40 * dpi_scale;
    const int gap          = 10 * dpi_scale;
    const int line_h       = 14 * text_scale;

    const int w1     = overlay_text_measure(line1, text_scale);
    const int w2     = overlay_text_measure(line2, text_scale);
    const int text_w = utils_max(w1, w2);

    const int text_h    = 2 * line_h;
    const int content_h = utils_max(box_size, text_h);

    const int panel_w = pad + box_size + gap + text_w + pad;
    const int panel_h = pad + content_h + pad;

    const int offset = (int)(14 * dpi_scale);
    int px_pos       = mx + offset;
    int py_pos       = my + offset;
    if (px_pos + panel_w > win.x)
    {
        px_pos = mx - panel_w - offset;
    }
    if (py_pos + panel_h > win.y)
    {
        py_pos = my - panel_h - offset;
    }
    px_pos = utils_clamp(px_pos, 0, utils_max(0, win.x - panel_w));
    py_pos = utils_clamp(py_pos, 0, utils_max(0, win.y - panel_h));

    rect_s panel = {px_pos, py_pos, px_pos + panel_w, py_pos + panel_h};
    overlay_rect_draw(panel, UI_PANEL_BG_STRONG);

    // Квадрат цвета
    int box_y        = py_pos + pad + (content_h - box_size) / 2;
    rect_s pix_color = {px_pos + pad, box_y, px_pos + pad + box_size, box_y + box_size};
    overlay_rect_draw(pix_color, (fcolor_rgba_s) {px.r / 255.0f, px.g / 255.0f, px.b / 255.0f, 1.0f});
    overlay_rect_outline_draw(pix_color, UI_PANEL_BORDER, 1);

    // Текст
    int text_y = py_pos + pad + (content_h - text_h) / 2;
    int tx     = px_pos + pad + box_size + gap;
    overlay_text_draw((point_s) {tx, text_y}, line1, UI_TEXT_PRIMARY, text_scale);
    overlay_text_draw((point_s) {tx, text_y + line_h}, line2, UI_TEXT_PRIMARY, text_scale);
}
