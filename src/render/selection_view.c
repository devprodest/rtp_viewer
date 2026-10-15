#include "selection_view.h"

#include "render/text_overlay.h"

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

    int sx0, sy0, sx1, sy1;
    selection_rect(sel, &sx0, &sy0, &sx1, &sy1);

    int ex0 = vp_x + (sx0 * vp_w) / frame_w;
    int ey0 = vp_y + (sy0 * vp_h) / frame_h;
    int ex1 = vp_x + (sx1 * vp_w) / frame_w;
    int ey1 = vp_y + (sy1 * vp_h) / frame_h;

    overlay_rect(ex0, ey0, ex1, ey1, 1.0f, 1.0f, 1.0f, 0.08f);
    overlay_rect_outline(ex0, ey0, ex1, ey1, 1.0f, 1.0f, 1.0f, 0.9f, 1);
}
