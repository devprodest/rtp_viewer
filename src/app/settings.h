#pragma once

#include <stdbool.h>

#include "util/types.h"

/// \brief Загружает настройки из rtp_viewer.ini рядом с exe.
///        Если файла нет или он битый — дефолтные значения.
void settings_load(void);

/// \brief Сохраняет текущее состояние в rtp_viewer.ini.
///        Идемпотентна: если файл уже актуален, ничего не пишет.
void settings_save(void);

/// \brief Помечает настройки как «изменённые». Реальная запись произойдёт
///        в ближайший settings_tick() (не чаще 1 раза в секунду).
void settings_mark_dirty(void);

/// \brief Вызывать из app_run в каждом кадре. Если есть dirty и прошло
///        больше 1 секунды с прошлого save — сохраняет.
void settings_tick(void);

// --- API для панелей ---

/// \brief Читает позицию панели. Возвращает false, если ключ не задан.
bool settings_get_panel_pos(const char * id, point_s * out_pos);

/// \brief Читает флаг видимости. Возвращает false, если ключ не задан.
bool settings_get_panel_enabled(const char * id, bool * out_enabled);

void settings_set_panel_pos(const char * id, point_s pos);
void settings_set_panel_enabled(const char * id, bool enabled);
bool settings_get_panel_anchor(const char * id, int * out_h, int * out_v);
void settings_set_panel_anchor(const char * id, int h, int v);
