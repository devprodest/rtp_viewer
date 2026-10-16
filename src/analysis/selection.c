#include "selection.h"

#define SELECTION_MIN_SIZE 4

void selection_init(selection_s * s)
{
    *s = (selection_s) {};
}

void selection_clear(selection_s * s)
{
    selection_init(s);
}

void selection_begin(selection_s * s, point_s p)
{
    s->active    = false;
    s->dragging  = true;
    s->select.x0 = p.x;
    s->select.y0 = p.y;
    s->select.x1 = p.x;
    s->select.y1 = p.y;
}

void selection_update(selection_s * s, point_s p)
{
    if (!s->dragging)
    {
        return;
    }
    s->select.x1 = p.x;
    s->select.y1 = p.y;
}

void selection_end(selection_s * s)
{
    if (!s->dragging)
    {
        return;
    }
    s->dragging = false;

    rect_s select = selection_rect(s);
    if ((select.x1 - select.x0) < SELECTION_MIN_SIZE || (select.y1 - select.y0) < SELECTION_MIN_SIZE)
    {
        selection_clear(s);
        return;
    }
    s->active = true;
}

rect_s selection_rect(const selection_s * s)
{
    return (rect_s) {
        .x0 = (s->select.x0 < s->select.x1) ? s->select.x0 : s->select.x1,
        .y0 = (s->select.y0 < s->select.y1) ? s->select.y0 : s->select.y1,
        .x1 = (s->select.x0 > s->select.x1) ? s->select.x0 : s->select.x1,
        .y1 = (s->select.y0 > s->select.y1) ? s->select.y0 : s->select.y1,
    };
}

bool selection_is_meaningful(const selection_s * s)
{
    if (!s->active)
    {
        return false;
    }
    
    rect_s select = selection_rect(s);
    return (select.x1 - select.x0) >= SELECTION_MIN_SIZE && //
           (select.y1 - select.y0) >= SELECTION_MIN_SIZE;
}
