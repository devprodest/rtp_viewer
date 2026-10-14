#pragma once

#include <stdbool.h>

typedef struct selection_s
{
    bool active;   ///< Выделение установлено
    bool dragging; ///< Идёт перетаскивание
    int  x0;       ///< Начало по X (координаты кадра)
    int  y0;       ///< Начало по Y
    int  x1;       ///< Конец по X
    int  y1;       ///< Конец по Y
} selection_s;

void selection_init(selection_s * s);
void selection_clear(selection_s * s);
void selection_begin(selection_s * s, int x, int y);
void selection_update(selection_s * s, int x, int y);
void selection_end(selection_s * s);
void selection_rect(const selection_s * s, int * x0, int * y0, int * x1, int * y1);
bool selection_is_meaningful(const selection_s * s);