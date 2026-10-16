#pragma once

#include <stdbool.h>
#include "util/types.h"

typedef struct selection_s
{
    rect_s select;
    bool active;   ///< Выделение установлено
    bool dragging; ///< Идёт перетаскивание
} selection_s;

void selection_init(selection_s * s);
void selection_clear(selection_s * s);

void selection_begin(selection_s * s,  point_s p);
void selection_update(selection_s * s, point_s p);
void selection_end(selection_s * s);

rect_s selection_rect(const selection_s * s);
bool selection_is_meaningful(const selection_s * s);