#pragma once

/// \file ui_color_scheme.h
/// \brief Единая цветовая схема UI.
///
/// \details
/// Слои:
///   1. Базовая палитра (UI_RGB_*, UI_RGBA_*) — именованные цвета.
///   2. Хелперы UI_RGBA_FROM_RGB / UI_RGB_FROM_RGBA — конвертация.
///   3. Семантические макросы (UI_TEXT_*, UI_PANEL_*, UI_MENU_*, ...) —
///      то, что реально используется в коде UI.
///
/// `fcolor_rgb_s`  — для overlay_text_draw (без альфы).
/// `fcolor_rgba_s` — для overlay_rect_draw / outline_draw.
/// Все макросы — compound literals, передаются в функции по значению.

#include "util/types.h"

//------------------------------------------------ Базовые цвета (RGB) -------------------------------------------------

#define UI_RGB_BLACK                 ((fcolor_rgb_s) {0.00f, 0.00f, 0.00f})
#define UI_RGB_WHITE                 ((fcolor_rgb_s) {1.00f, 1.00f, 1.00f})
#define UI_RGB_RED                   ((fcolor_rgb_s) {1.00f, 0.00f, 0.00f})
#define UI_RGB_GREEN                 ((fcolor_rgb_s) {0.00f, 1.00f, 0.00f})
#define UI_RGB_BLUE                  ((fcolor_rgb_s) {0.00f, 0.00f, 1.00f})
#define UI_RGB_YELLOW                ((fcolor_rgb_s) {1.00f, 1.00f, 0.00f})
#define UI_RGB_CYAN                  ((fcolor_rgb_s) {0.00f, 1.00f, 1.00f})
#define UI_RGB_MAGENTA               ((fcolor_rgb_s) {1.00f, 0.00f, 1.00f})
#define UI_RGB_ORANGE                ((fcolor_rgb_s) {1.00f, 0.60f, 0.00f})
#define UI_RGB_PURPLE                ((fcolor_rgb_s) {0.60f, 0.30f, 0.80f})

//---- Серые (шкала) ---------------------------------------------------------------------------------------------------

#define UI_RGB_GRAY_DARK             ((fcolor_rgb_s) {0.25f, 0.25f, 0.25f})
#define UI_RGB_GRAY_DIM              ((fcolor_rgb_s) {0.40f, 0.40f, 0.40f})
#define UI_RGB_GRAY                  ((fcolor_rgb_s) {0.50f, 0.50f, 0.50f})
#define UI_RGB_GRAY_SILVER           ((fcolor_rgb_s) {0.55f, 0.55f, 0.55f})
#define UI_RGB_GRAY_LIGHT            ((fcolor_rgb_s) {0.80f, 0.80f, 0.80f})

//----------------------------------------- Базовые цвета (RGBA, alpha = 1) --------------------------------------------

#define UI_RGBA_BLACK                ((fcolor_rgba_s) {0.00f, 0.00f, 0.00f, 1.0f})
#define UI_RGBA_WHITE                ((fcolor_rgba_s) {1.00f, 1.00f, 1.00f, 1.0f})
#define UI_RGBA_RED                  ((fcolor_rgba_s) {1.00f, 0.00f, 0.00f, 1.0f})
#define UI_RGBA_GREEN                ((fcolor_rgba_s) {0.00f, 1.00f, 0.00f, 1.0f})
#define UI_RGBA_BLUE                 ((fcolor_rgba_s) {0.00f, 0.00f, 1.00f, 1.0f})
#define UI_RGBA_YELLOW               ((fcolor_rgba_s) {1.00f, 1.00f, 0.00f, 1.0f})
#define UI_RGBA_CYAN                 ((fcolor_rgba_s) {0.00f, 1.00f, 1.00f, 1.0f})
#define UI_RGBA_MAGENTA              ((fcolor_rgba_s) {1.00f, 0.00f, 1.00f, 1.0f})
#define UI_RGBA_ORANGE               ((fcolor_rgba_s) {1.00f, 0.60f, 0.00f, 1.0f})
#define UI_RGBA_PURPLE               ((fcolor_rgba_s) {0.60f, 0.30f, 0.80f, 1.0f})

#define UI_RGBA_GRAY_DARK            ((fcolor_rgba_s) {0.25f, 0.25f, 0.25f, 1.0f})
#define UI_RGBA_GRAY_DIM             ((fcolor_rgba_s) {0.40f, 0.40f, 0.40f, 1.0f})
#define UI_RGBA_GRAY                 ((fcolor_rgba_s) {0.50f, 0.50f, 0.50f, 1.0f})
#define UI_RGBA_GRAY_SILVER          ((fcolor_rgba_s) {0.55f, 0.55f, 0.55f, 1.0f})
#define UI_RGBA_GRAY_LIGHT           ((fcolor_rgba_s) {0.80f, 0.80f, 0.80f, 1.0f})

#define UI_RGBA_TRANSPARENT          ((fcolor_rgba_s) {0.00f, 0.00f, 0.00f, 0.0f})

//----------------------------------------- Хелперы: RGB <-> RGBA ------------------------------------------------------

#define UI_RGBA_FROM_RGB(rgb, alpha) ((fcolor_rgba_s) {(rgb).r, (rgb).g, (rgb).b, (alpha)})
#define UI_RGB_FROM_RGBA(rgba)       ((fcolor_rgb_s) {(rgba).r, (rgba).g, (rgba).b})

//--------------------------------------------- Семантика: текст -------------------------------------------------------

#define UI_TEXT_PRIMARY              UI_RGB_WHITE
#define UI_TEXT_SECONDARY            UI_RGB_GRAY_LIGHT

//--------------------------------------------- Семантика: панели ------------------------------------------------------

#define UI_PANEL_BG                  UI_RGBA_FROM_RGB(UI_RGB_BLACK, 0.55f)
#define UI_PANEL_BG_STRONG           UI_RGBA_FROM_RGB(UI_RGB_BLACK, 0.60f)
#define UI_PANEL_BORDER              UI_RGBA_WHITE

//--------------------------------------------- Семантика: прогресс-бар ------------------------------------------------

#define UI_PROGRESS_PANEL_BG         ((fcolor_rgba_s) {0.15f, 0.15f, 0.15f, 0.35f})
#define UI_PROGRESS_BG               ((fcolor_rgba_s) {0.15f, 0.15f, 0.15f, 0.85f})
#define UI_PROGRESS_FILL             ((fcolor_rgba_s) {0.20f, 0.50f, 0.20f, 0.85f})
#define UI_PROGRESS_BORDER           UI_RGBA_GRAY_SILVER

//--------------------------------------------- Семантика: crosshair ---------------------------------------------------

#define UI_CROSSHAIR_LINE            UI_RGBA_FROM_RGB(UI_RGB_GREEN, 0.75f)

//--------------------------------------------- Семантика: контекстное меню --------------------------------------------

//---- Ячейки панели (кнопки) ------------------------------------------------------------------------------------------

#define UI_CELL_BG                   ((fcolor_rgba_s) {0.22f, 0.22f, 0.22f, 0.75f})
#define UI_CELL_BORDER               ((fcolor_rgba_s) {0.35f, 0.35f, 0.35f, 1.00f})
#define UI_MENU_BG                   ((fcolor_rgba_s) {0.15f, 0.15f, 0.15f, 0.75f})
#define UI_MENU_BORDER               UI_RGBA_GRAY
#define UI_MENU_SEPARATOR            UI_RGBA_GRAY_DIM
#define UI_MENU_ITEM_HOVER           ((fcolor_rgba_s) {0.25f, 0.40f, 0.25f, 1.00f})
#define UI_MENU_ITEM_ACTIVE          ((fcolor_rgba_s) {0.20f, 0.50f, 0.20f, 1.00f})
#define UI_MENU_ITEM_ACTIVE_HOVER    ((fcolor_rgba_s) {0.30f, 0.70f, 0.30f, 1.00f})

//---- Фоновые оттенки для групп меню ----------------------------------------------------------------------------------
// Очень приглушённые, чтобы не перебивать hover/active (они ярко-зелёные).
// Разные по оттенку, но одинаковые по светлоте — визуально «ровный» ряд плиток.

#define UI_CELL_BG_DISPLAY           ((fcolor_rgba_s) {0.16f, 0.20f, 0.28f, 0.95f}) // синеватый — отображение
#define UI_CELL_BG_ANALYSIS          ((fcolor_rgba_s) {0.22f, 0.18f, 0.28f, 0.95f}) // фиолетовый — анализ
#define UI_CELL_BG_DIFF              ((fcolor_rgba_s) {0.18f, 0.24f, 0.16f, 0.95f}) // зеленоватый — различие
#define UI_CELL_BG_PLAYBACK          ((fcolor_rgba_s) {0.16f, 0.24f, 0.24f, 0.95f}) // бирюзовый — воспроизведение
#define UI_CELL_BG_RECORDING         ((fcolor_rgba_s) {0.24f, 0.16f, 0.16f, 0.95f}) // красноватый — запись
#define UI_CELL_BG_UTILITY           ((fcolor_rgba_s) {0.24f, 0.22f, 0.18f, 0.95f}) // тёплый серый — утилиты

//--------------------------------------------- Семантика: выделение ---------------------------------------------------

#define UI_SELECTION_LINE            UI_RGBA_WHITE
#define UI_SELECTION_FILL            UI_RGBA_FROM_RGB(UI_RGB_WHITE, 0.08f)
