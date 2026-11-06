#include "context_menu.h"

#include "control/commands.h"
#include "render/overlay.h"
#include "render/ui/svg_icons.h"
#include "render/ui/ui_color_scheme.h"

#include <stdio.h>

//-------------------------------------------------- Панель -----------------------------------------------------------

/// Количество колонок в сетке кнопок.
#define PANEL_COLS     6
/// Сторона одной ячейки (кнопки) в логических пикселях (до dpi-скейла).
#define CELL_SIZE      64
/// Размер иконки внутри ячейки в пикселях.
#define ICON_SIZE      42
/// Отступ от края панели до крайних ячеек.
#define PANEL_PADDING  4
/// Базовый масштаб шрифта подписи под иконкой.
#define TEXT_SCALE     0.75f
/// Расстояние между соседними ячейками.
#define CELL_OUTER_PAD 2
/// Внутренний отступ внутри ячейки.
#define CELL_INNER_PAD 4
/// Высота нижней панели с подсказкой.
#define TOOLTIP_HEIGHT 28

//----------------------------------------------------------------------------------------------------------------------

typedef enum menu_group_e
{
    MENU_GROUP_DISPLAY,
    MENU_GROUP_ANALYSIS,
    MENU_GROUP_DIFF,
    MENU_GROUP_PLAYBACK,
    MENU_GROUP_RECORDING,
    MENU_GROUP_UTILITY,
} menu_group_e;

typedef struct context_menu_item_s
{
    const char * label;
    commands_e cmd;
    bool is_toggle;
    menu_group_e group;
    const char * hotkey;
} context_menu_item_s;

#define CMD_NONE ((commands_e) - 1)

static const context_menu_item_s g_items[] = {
    {"Sobel Mask",      CMD_TOGGLE_GRADIENT,        true,  MENU_GROUP_ANALYSIS,  "F"     },
    {"Inspector",       CMD_TOGGLE_INSPECTOR,       true,  MENU_GROUP_ANALYSIS,  "I"     },
    {"Saturation",      CMD_TOGGLE_SATURATION,      true,  MENU_GROUP_ANALYSIS,  "E"     },
    {"Histogram",       CMD_TOGGLE_HISTOGRAM,       true,  MENU_GROUP_ANALYSIS,  "H"     },
    {"Log Scale",       CMD_TOGGLE_HISTOGRAM_SCALE, true,  MENU_GROUP_ANALYSIS,  "Ctrl+H"},
    {"Stats",           CMD_TOGGLE_STATS,           true,  MENU_GROUP_ANALYSIS,  "T"     },

    {"Line Profile",    CMD_TOGGLE_LINE_PROFILE,    true,  MENU_GROUP_ANALYSIS,  "L"     },
    {"Vectorscope",     CMD_TOGGLE_VECTORSCOPE,     true,  MENU_GROUP_ANALYSIS,  "M"     },
    {"Diff Ref",        CMD_SET_REFERENCE,          false, MENU_GROUP_DIFF,      "V"     },
    {"Diff View",       CMD_TOGGLE_DIFF,            true,  MENU_GROUP_DIFF,      "B"     },
    {"Diff Gain -",     CMD_DIFF_GAIN_DOWN,         false, MENU_GROUP_DIFF,      "-"     },
    {"Diff Gain +",     CMD_DIFF_GAIN_UP,           false, MENU_GROUP_DIFF,      "="     },

    {"Backward",        CMD_STEP_BACKWARD,          false, MENU_GROUP_PLAYBACK,  "Left"  },
    {"Pause",           CMD_TOGGLE_PAUSE,           true,  MENU_GROUP_PLAYBACK,  "Space" },
    {"Forward",         CMD_STEP_FORWARD,           false, MENU_GROUP_PLAYBACK,  "Right" },
    {"Recording",       CMD_TOGGLE_RECORDING,       true,  MENU_GROUP_RECORDING, "R"     },
    {"Flush Save",      CMD_FLUSH_SAVER,            false, MENU_GROUP_RECORDING, "W"     },
    {"Screenshot",      CMD_REQUEST_SCREENSHOT,     false, MENU_GROUP_RECORDING, "S"     },

    {"Auto-levels",     CMD_TOGGLE_AUTO_LEVELS,     true,  MENU_GROUP_DISPLAY,   "2"     },
    {"Smoothing",       CMD_TOGGLE_SMOOTHING,       true,  MENU_GROUP_DISPLAY,   "1"     },
    {"Clear Selection", CMD_CLEAR_SELECTION,        false, MENU_GROUP_UTILITY,   "Esc"   },
    {"Reset Zoom",      CMD_ZOOM_RESET,             false, MENU_GROUP_UTILITY,   "0"     },
    {"Full screen",     CMD_TOGGLE_FULLSCREEN,      true,  MENU_GROUP_UTILITY,   "F11"   },
    {"Help",            CMD_SHOW_HELP,              false, MENU_GROUP_UTILITY,   "F1"    },
};

static const int g_item_count = (int)(sizeof(g_items) / sizeof(g_items[ 0 ]));

static bool g_open     = false;
static point_s g_pos   = {0};
static point_s g_hover = {-1, -1};

void context_menu_open(point_s pos)
{
    g_open = true;
    g_pos  = pos;
}
void context_menu_close(void)
{
    g_open = false;
}
bool context_menu_is_open(void)
{
    return g_open;
}
void context_menu_set_hover(point_s mouse)
{
    g_hover = mouse;
}

//------------------------------------------------- Иконки (примитивы) -------------------------------------------------

static fcolor_rgba_s group_bg_color(menu_group_e group)
{
    switch (group)
    {
        case MENU_GROUP_DISPLAY: return UI_CELL_BG_DISPLAY;
        case MENU_GROUP_ANALYSIS: return UI_CELL_BG_ANALYSIS;
        case MENU_GROUP_DIFF: return UI_CELL_BG_DIFF;
        case MENU_GROUP_PLAYBACK: return UI_CELL_BG_PLAYBACK;
        case MENU_GROUP_RECORDING: return UI_CELL_BG_RECORDING;
        case MENU_GROUP_UTILITY: return UI_CELL_BG_UTILITY;
    }
    return UI_CELL_BG;
}

static void draw_icon(point_s center, float scale)
{
    int s           = (int)(ICON_SIZE * scale);
    fcolor_rgba_s c = UI_RGBA_WHITE;

    overlay_rect_outline_draw((rect_s) {center.x - s / 3, center.y - s / 3, center.x + s / 3, center.y + s / 3}, c, 1);
}

//----------------------------------------------- Отрисовка панели -----------------------------------------------------

static void get_panel_rect(image_size_s win, float scale, rect_s * out_panel, int * out_visible_count)
{
    int cols    = PANEL_COLS;
    int visible = g_item_count;

    int rows  = (visible + cols - 1) / cols;
    int pad   = (int)(PANEL_PADDING * scale);
    int tip_h = (int)(TOOLTIP_HEIGHT * scale);

    int w = cols * (int)(CELL_SIZE * scale) + 2 * pad;
    int h = rows * (int)(CELL_SIZE * scale) + 2 * pad + tip_h;

    int x = (win.x - w) / 2;
    int y = (win.y - h) / 2;

    out_panel->x0 = x;
    out_panel->y0 = y;
    out_panel->x1 = x + w;
    out_panel->y1 = y + h;
    if (out_visible_count)
    {
        *out_visible_count = visible;
    }
}

void context_menu_draw(image_size_s win, float scale)
{
    if (!g_open)
    {
        return;
    }

    rect_s panel;
    int visible_count = 0;
    get_panel_rect(win, scale, &panel, &visible_count);

    // Фон панели
    overlay_rect_draw(panel, UI_MENU_BG);

    int cols      = PANEL_COLS;
    int cell_w    = (int)(CELL_SIZE * scale);
    int cell_h    = (int)(CELL_SIZE * scale);
    int outer_pad = (int)(CELL_OUTER_PAD * scale);
    int inner_pad = (int)(CELL_INNER_PAD * scale);
    int panel_pad = (int)(PANEL_PADDING * scale);

    int start_x     = panel.x0 + panel_pad;
    int start_y     = panel.y0 + panel_pad;
    int idx         = 0;
    int hovered_idx = -1;

    for (int i = 0; i < g_item_count; i++)
    {
        int row = idx / cols;
        int col = idx % cols;
        idx++;

        if (g_items[ i ].cmd == CMD_NONE)
        {
            continue;
        }

        int cx = start_x + col * cell_w;
        int cy = start_y + row * cell_h;

        rect_s cell = {cx + outer_pad, cy + outer_pad, cx + cell_w - outer_pad, cy + cell_h - outer_pad};

        bool hovered = (g_hover.x >= cell.x0 && g_hover.x < cell.x1 && g_hover.y >= cell.y0 && g_hover.y < cell.y1);
        bool active  = g_items[ i ].is_toggle && cmd_is_active(g_items[ i ].cmd);

        if (hovered)
        {
            hovered_idx = i;
        }

        if (active)
        {
            overlay_rect_draw(cell, hovered ? UI_MENU_ITEM_ACTIVE_HOVER : UI_MENU_ITEM_ACTIVE);
        }
        else if (hovered)
        {
            overlay_rect_draw(cell, UI_MENU_ITEM_HOVER);
        }
        else
        {
            overlay_rect_draw(cell, group_bg_color(g_items[ i ].group));
        }

        overlay_rect_outline_draw(cell, UI_CELL_BORDER, 1);

        // Иконка (fallback)
        if (!svg_icon_exists(g_items[ i ].cmd))
        {
            point_s icon_center = {cx + cell_w / 2, cy + cell_h / 2 - 8};
            draw_icon(icon_center, scale);
        }

        // Текст
        const char * label = g_items[ i ].label;
        float fit_scale    = TEXT_SCALE * scale;
        int tw             = overlay_text_measure(label, fit_scale);
        int max_w          = (cell.x1 - cell.x0) - 2 * inner_pad;
        while (tw > max_w && fit_scale > 0.45f)
        {
            fit_scale -= 0.05f;
            tw = overlay_text_measure(label, fit_scale);
        }

        int tx = cell.x0 + (cell.x1 - cell.x0 - tw) / 2;
        int ty = cell.y1 - inner_pad - (int)(16 * fit_scale);
        overlay_text_draw((point_s) {tx, ty}, label, UI_TEXT_PRIMARY, fit_scale);
    }

    // --- Тултип (нижняя панель) ---
    if (hovered_idx >= 0)
    {
        int tip_h         = (int)(TOOLTIP_HEIGHT * scale);
        rect_s tooltip_bg = {panel.x0, panel.y1 - tip_h, panel.x1, panel.y1};

        char tip_buf[ 128 ];
        const char * hotkey = g_items[ hovered_idx ].hotkey;

        if (hotkey && hotkey[ 0 ] != '\0')
        {
            snprintf(tip_buf, sizeof(tip_buf), "%s  [%s]", g_items[ hovered_idx ].label, hotkey);
        }
        else
        {
            snprintf(tip_buf, sizeof(tip_buf), "%s", g_items[ hovered_idx ].label);
        }

        float tip_scale = 0.85f * scale;
        int tw          = overlay_text_measure(tip_buf, tip_scale);
        int tx          = tooltip_bg.x0 + (tooltip_bg.x1 - tooltip_bg.x0 - tw) / 2;

        // Центрируем по вертикали
        int ty = tooltip_bg.y0 + (tooltip_bg.y1 - tooltip_bg.y0 - (int)(14 * tip_scale)) / 2;

        overlay_text_draw((point_s) {tx, ty}, tip_buf, UI_TEXT_PRIMARY, tip_scale);
    }
}

bool context_menu_handle_click(point_s mouse, image_size_s win, float scale)
{
    if (!g_open)
    {
        return false;
    }

    rect_s panel;
    get_panel_rect(win, scale, &panel, nullptr);

    if (mouse.x < panel.x0 || mouse.x >= panel.x1 || mouse.y < panel.y0 || mouse.y >= panel.y1)
    {
        context_menu_close();
        return true;
    }

    int tip_h = (int)(TOOLTIP_HEIGHT * scale);
    // Клик по зоне тултипа просто поглощаем, чтобы меню не закрывалось случайно
    if (mouse.y >= panel.y1 - tip_h)
    {
        return true;
    }

    int cols      = PANEL_COLS;
    int cell_w    = (int)(CELL_SIZE * scale);
    int cell_h    = (int)(CELL_SIZE * scale);
    int outer_pad = (int)(CELL_OUTER_PAD * scale);
    int panel_pad = (int)(PANEL_PADDING * scale);

    int start_x = panel.x0 + panel_pad;
    int start_y = panel.y0 + panel_pad;
    int idx     = 0;

    for (int i = 0; i < g_item_count; i++)
    {
        int row = idx / cols;
        int col = idx % cols;
        idx++;

        if (g_items[ i ].cmd == CMD_NONE)
        {
            continue;
        }

        int cx = start_x + col * cell_w;
        int cy = start_y + row * cell_h;

        rect_s cell = {cx + outer_pad, cy + outer_pad, cx + cell_w - outer_pad, cy + cell_h - outer_pad};

        if (mouse.x >= cell.x0 && mouse.x < cell.x1 && mouse.y >= cell.y0 && mouse.y < cell.y1)
        {
            cmd_execute(g_items[ i ].cmd);
            context_menu_close();
            return true;
        }
    }

    context_menu_close();
    return true;
}

void context_menu_draw_icons(image_size_s win, float scale)
{
    if (!g_open)
    {
        return;
    }

    rect_s panel;
    get_panel_rect(win, scale, &panel, nullptr);

    int cols      = PANEL_COLS;
    int cell_w    = (int)(CELL_SIZE * scale);
    int cell_h    = (int)(CELL_SIZE * scale);
    int inner_pad = (int)(CELL_INNER_PAD * scale);
    int panel_pad = (int)(PANEL_PADDING * scale);

    int start_x = panel.x0 + panel_pad;
    int start_y = panel.y0 + panel_pad;
    int idx     = 0;

    for (int i = 0; i < g_item_count; i++)
    {
        int row = idx / cols;
        int col = idx % cols;
        idx++;

        if (g_items[ i ].cmd == CMD_NONE)
        {
            continue;
        }

        int cx              = start_x + col * cell_w;
        int cy              = start_y + row * cell_h;
        int icon_s          = (int)(ICON_SIZE * scale);
        int icon_top        = cy + (int)(CELL_OUTER_PAD * scale) + inner_pad;
        point_s icon_center = {cx + cell_w / 2, icon_top + icon_s / 2};

        svg_icons_draw(g_items[ i ].cmd, icon_center, (float)icon_s, UI_RGBA_WHITE);
    }
}
