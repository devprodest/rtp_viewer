#include "selection_view.h"

#include "render/overlay.h"

#define SELECTION_COLOR 1.0f, 1.0f, 1.0f


void selection_view_draw(const selection_s * sel, int vp_x, int vp_y, int vp_w, int vp_h, int frame_w, int frame_h)
{
    if (!sel || (!sel->active && !sel->dragging))
    {
        return;
    }

    if (frame_w <= 0 || frame_h <= 0 || vp_w <= 0 || vp_h <= 0)
    {
        return;
    }

    rect_s select = selection_rect(sel);
    rect_s rect   = {
        .x0 = vp_x + (select.x0 * vp_w) / frame_w,
        .y0 = vp_y + (select.y0 * vp_h) / frame_h,
        .x1 = vp_x + (select.x1 * vp_w) / frame_w,
        .y1 = vp_y + (select.y1 * vp_h) / frame_h,
    };

    fcolor_rgb_s color = {1.0f, 1.0f, 1.0f};

    overlay_rect(rect, to_rgba_convert(fcolor_rgba_s, color, 0.08f));
    overlay_rect_outline(rect, to_rgba_convert(fcolor_rgba_s, color, 0.9f), 1);
}
