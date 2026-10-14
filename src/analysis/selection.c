#include "selection.h"

#define SELECTION_MIN_SIZE 4

void selection_init(selection_s * s)
{
    s->active   = false;
    s->dragging = false;
    s->x0 = s->y0 = 0;
    s->x1 = s->y1 = 0;
}

void selection_clear(selection_s * s)
{
    selection_init(s);
}

void selection_begin(selection_s * s, int x, int y)
{
    s->active   = false;
    s->dragging = true;
    s->x0       = x;
    s->y0       = y;
    s->x1       = x;
    s->y1       = y;
}

void selection_update(selection_s * s, int x, int y)
{
    if (!s->dragging)
    {
        return;
    }
    s->x1 = x;
    s->y1 = y;
}

void selection_end(selection_s * s)
{
    if (!s->dragging)
    {
        return;
    }
    s->dragging = false;

    int ax0, ay0, ax1, ay1;
    selection_rect(s, &ax0, &ay0, &ax1, &ay1);

    if ((ax1 - ax0) < SELECTION_MIN_SIZE || (ay1 - ay0) < SELECTION_MIN_SIZE)
    {
        selection_clear(s);
        return;
    }
    s->active = true;
}

void selection_rect(const selection_s * s, int * x0, int * y0, int * x1, int * y1)
{
    *x0 = (s->x0 < s->x1) ? s->x0 : s->x1;
    *y0 = (s->y0 < s->y1) ? s->y0 : s->y1;
    *x1 = (s->x0 > s->x1) ? s->x0 : s->x1;
    *y1 = (s->y0 > s->y1) ? s->y0 : s->y1;
}

bool selection_is_meaningful(const selection_s * s)
{
    if (!s->active)
    {
        return false;
    }
    int x0, y0, x1, y1;
    selection_rect(s, &x0, &y0, &x1, &y1);
    return (x1 - x0) >= SELECTION_MIN_SIZE && (y1 - y0) >= SELECTION_MIN_SIZE;
}
