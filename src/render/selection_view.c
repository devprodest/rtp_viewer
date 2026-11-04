#include "selection_view.h"
#include "render/overlay.h"
#include "ui/ui_color_scheme.h"

#define SELECTION_COLOR 1.0f, 1.0f, 1.0f


void selection_view_draw(const selection_s * sel, point_s vpoint, image_size_s vsize, image_size_s win)
{
    if (!sel || (!sel->active && !sel->dragging))
    {
        return;
    }
    if (win.x <= 0 || win.y <= 0 || vsize.x <= 0 || vsize.y <= 0)
    {
        return;
    }

    rect_s select = selection_rect(sel);
    rect_s rect   = {
        .x0 = vpoint.x + (select.x0 * vsize.x) / win.x,
        .y0 = vpoint.y + (select.y0 * vsize.y) / win.y,
        .x1 = vpoint.x + (select.x1 * vsize.x) / win.x,
        .y1 = vpoint.y + (select.y1 * vsize.y) / win.y,
    };

    fcolor_rgba_s color = UI_SELECTION_FILL;
    fcolor_rgba_s color_line = UI_SELECTION_LINE;

    overlay_rect_draw(rect, color);
    overlay_rect_outline_draw(rect, color_line, 1);
}
