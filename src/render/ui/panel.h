#pragma once

#include <stdbool.h>

#include "util/types.h"

/// \brief Высота title bar в логических пикселях (до dpi-скейла).
#define PANEL_TITLE_H_BASE 24

/// \brief Высота title bar с учётом dpi.
int panel_title_height(float dpi_scale);

/// \brief Описание одной перетаскиваемой панели.
typedef struct panel_s
{
    const char * id;                           ///< "histogram", "vectorscope", "stats"
    const char * title;                        ///< Заголовок в title bar
    point_s (*get_pos)(void);                  ///< Текущая позиция (верхний-левый угол ВСЕЙ панели)
    void (*set_pos)(point_s pos);              ///< Установить позицию
    image_size_s (*get_size)(float dpi_scale); ///< Размер КОНТЕНТА (без title bar)
    bool (*is_visible)(void);                  ///< Активна ли панель
    void (*on_toggle)(void);                   ///< Toggle модуля (для крестика и settings)
} panel_s;

void panel_register(const panel_s * p);
void panel_unregister_all(void);

/// \brief Обработка мыши. Возвращает true, если событие поглощено панелью
///        (клик по title bar, крестику или drag).
bool panel_handle_mouse(point_s mouse, bool left_down, bool menu_open, image_size_s win, float dpi_scale);

/// \brief Рисует chrome (title bar, рамка, крестик) всех видимых панелей.
///        Вызывается внутри overlay_begin/overlay_end ДО отрисовки контента.
void panel_draw_chrome(image_size_s win, float dpi_scale);

/// \brief Сдвигает панели внутрь окна, если они вылезли за границы.
void panel_clamp_to_window(image_size_s win, float dpi_scale);

/// \brief Применяет сохранённые позиции и visible-флаги из settings.
void panel_apply_settings(void);

/// \brief Сохраняет позиции всех панелей в settings (полезно при cleanup).
void panel_save_settings(void);
