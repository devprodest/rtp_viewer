#pragma once

/// \file ui_color_scheme.h
/// \brief Единая цветовая схема UI.
///
/// \details
/// Слои:
///   1. Базовая палитра (UI_RGB_*, UI_RGBA_*) — именованные цвета.
///   2. Хелперы UI_RGBA_FROM_RGB / UI_RGB_FROM_RGBA — конвертация.
///   3. Семантические роли (UI_COLOR_*) — базовые сущности (панели, акценты, текст).
///   4. Привязки к элементам (UI_PANEL_*, UI_TEXT_*, ...) — чтобы не ломать старый код.

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

//============================================= СЕМАНТИЧЕСКИЕ РОЛИ =====================================================

//---- Поверхности (панели, фоны) --------------------------------------------------------------------------------------
// Единый базовый цвет для всех панелей (меню, статистика, профили, пиксель-проб).

#define UI_COLOR_BG_PANEL            ((fcolor_rgba_s) {0.12f, 0.12f, 0.12f, 0.65f})
#define UI_COLOR_BG_SURFACE          ((fcolor_rgba_s) {0.22f, 0.22f, 0.22f, 0.75f}) // ячейки меню, кнопки
#define UI_COLOR_BORDER              ((fcolor_rgba_s) {0.35f, 0.35f, 0.35f, 1.00f}) // рамки панелей и кнопок

//---- Текст -----------------------------------------------------------------------------------------------------------

#define UI_COLOR_TEXT_PRIMARY        UI_RGB_WHITE
#define UI_COLOR_TEXT_SECONDARY      UI_RGB_GRAY_LIGHT

//---- Акценты (графики, индикаторы) -----------------------------------------------------------------------------------

#define UI_COLOR_ACCENT_PRIMARY      UI_RGBA_CYAN            // главный акцент (min-границы, оси Y)
#define UI_COLOR_ACCENT_SECONDARY    UI_RGBA_MAGENTA         // второй акцент (max-границы)
#define UI_COLOR_ACCENT_X            UI_RGBA_RED             // ось X / горизонтальные профили
#define UI_COLOR_ACCENT_ACTIVE       ((fcolor_rgba_s) {0.20f, 0.50f, 0.20f, 1.00f}) // включенные тогглы
#define UI_COLOR_ACCENT_ACTIVE_HOVER ((fcolor_rgba_s) {0.30f, 0.70f, 0.30f, 1.00f}) // включенные тогглы под курсором
#define UI_COLOR_ACCENT_HOVER        ((fcolor_rgba_s) {0.25f, 0.40f, 0.25f, 1.00f}) // обычный ховер меню
#define UI_COLOR_ACCENT_PROGRESS     ((fcolor_rgba_s) {0.20f, 0.50f, 0.20f, 0.85f}) // заполнение прогресс-бара

//============================================= ПРИВЯЗКИ ЭЛЕМЕНТОВ =====================================================

//---- Текст ----

#define UI_TEXT_PRIMARY              UI_COLOR_TEXT_PRIMARY
#define UI_TEXT_SECONDARY            UI_COLOR_TEXT_SECONDARY

//---- Панели ----
// UI_PANEL_BG_STRONG оставлен как алиас для обратной совместимости, 
// чтобы не ломать старый код в меню и пробе.

#define UI_PANEL_BG                  UI_COLOR_BG_PANEL
#define UI_PANEL_BG_STRONG           UI_COLOR_BG_PANEL
#define UI_PANEL_BORDER              UI_COLOR_BORDER

//---- Прогресс-бар ----

#define UI_PROGRESS_PANEL_BG         UI_COLOR_BG_PANEL
#define UI_PROGRESS_BG               UI_COLOR_BG_SURFACE
#define UI_PROGRESS_FILL             UI_COLOR_ACCENT_PROGRESS
#define UI_PROGRESS_BORDER           UI_COLOR_BORDER

//---- Crosshair ----

#define UI_CROSSHAIR_LINE            UI_RGBA_FROM_RGB(UI_RGB_GREEN, 0.75f)

//---- Контекстное меню ----

#define UI_CELL_BG                   UI_COLOR_BG_SURFACE
#define UI_CELL_BORDER               UI_COLOR_BORDER
#define UI_MENU_BG                   UI_COLOR_BG_PANEL
#define UI_MENU_BORDER               UI_COLOR_BORDER
#define UI_MENU_SEPARATOR            UI_RGBA_GRAY_DIM
#define UI_MENU_ITEM_HOVER           UI_COLOR_ACCENT_HOVER
#define UI_MENU_ITEM_ACTIVE          UI_COLOR_ACCENT_ACTIVE
#define UI_MENU_ITEM_ACTIVE_HOVER    UI_COLOR_ACCENT_ACTIVE_HOVER

//---- Фоновые оттенки для групп меню ----

#define UI_CELL_BG_DISPLAY           ((fcolor_rgba_s) {0.16f, 0.20f, 0.28f, 0.95f})
#define UI_CELL_BG_ANALYSIS          ((fcolor_rgba_s) {0.22f, 0.18f, 0.28f, 0.95f})
#define UI_CELL_BG_DIFF              ((fcolor_rgba_s) {0.18f, 0.24f, 0.16f, 0.95f})
#define UI_CELL_BG_PLAYBACK          ((fcolor_rgba_s) {0.16f, 0.24f, 0.24f, 0.95f})
#define UI_CELL_BG_RECORDING         ((fcolor_rgba_s) {0.24f, 0.16f, 0.16f, 0.95f})
#define UI_CELL_BG_UTILITY           ((fcolor_rgba_s) {0.24f, 0.22f, 0.18f, 0.95f})

//---- Выделение ----

#define UI_SELECTION_LINE            UI_RGBA_WHITE
#define UI_SELECTION_FILL            UI_RGBA_FROM_RGB(UI_RGB_WHITE, 0.08f)

//---- Line Profile ----

#define UI_LINE_PROFILE_BG           UI_COLOR_BG_PANEL
#define UI_LINE_PROFILE_GRID         ((fcolor_rgba_s) {1.0f, 1.0f, 1.0f, 0.15f})
#define UI_LINE_PROFILE_X            UI_RGBA_FROM_RGB(UI_RGB_RED, 0.55f)
#define UI_LINE_PROFILE_Y            UI_RGBA_FROM_RGB(UI_RGB_CYAN, 0.55f)

//---- Histogram ----

#define UI_HIST_EDGE_MIN             UI_COLOR_ACCENT_PRIMARY
#define UI_HIST_EDGE_MAX             UI_COLOR_ACCENT_SECONDARY