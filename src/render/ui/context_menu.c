#include "context_menu.h"

#include "control/commands.h"
#include "render/overlay.h"
#include "render/ui/ui_color_scheme.h"

//-------------------------------------------------- Панель -----------------------------------------------------------

/// Количество колонок в сетке кнопок.
#define PANEL_COLS     6
/// Сторона одной ячейки (кнопки) в логических пикселях (до dpi-скейла). Иконка и текст располагаются внутри этой
/// области.
#define CELL_SIZE      80
/// Размер иконки внутри ячейки в пикселях (до dpi-скейла). Обычно заметно меньше CELL_SIZE, чтобы осталось место под
/// текст.
#define ICON_SIZE      58
/// Отступ от края панели до крайних ячеек (до dpi-скейла). Формирует «воздух» вокруг сетки внутри полупрозрачного фона.
#define PANEL_PADDING  4
/// Базовый масштаб шрифта подписи под иконкой. Меньше 1.0f, чтобы длинные названия («Clear Selection») влезали в ширину
/// ячейки. Для совсем длинных — авто-фит уменьшает дальше.
#define TEXT_SCALE     0.75f
/// Расстояние между соседними ячейками (до dpi-скейла). Слишком большое — сетка «разваливается», слишком маленькое —
/// кнопки сливаются в один блок.
#define CELL_OUTER_PAD 2
/// Внутренний отступ внутри ячейки от её края до иконки/текста (до dpi-скейла). Не даёт контенту прилипать к рамке
/// кнопки.
#define CELL_INNER_PAD 4

//----------------------------------------------------------------------------------------------------------------------

typedef struct context_menu_item_s
{
    const char * label;
    commands_e cmd;
    bool is_toggle;
} context_menu_item_s;

static const context_menu_item_s g_items[] = {
    {"Smoothing",       CMD_TOGGLE_SMOOTHING,       true },
    {"Auto-levels",     CMD_TOGGLE_AUTO_LEVELS,     true },
    {"Saturation",      CMD_TOGGLE_SATURATION,      true },
    {"Histogram",       CMD_TOGGLE_HISTOGRAM,       true },
    {"Log Scale",       CMD_TOGGLE_HISTOGRAM_SCALE, true },
    {"CDF",             CMD_TOGGLE_HISTOGRAM_CDF,   true },
    {"Gradient",        CMD_TOGGLE_GRADIENT,        true },
    {"Inspector",       CMD_TOGGLE_INSPECTOR,       true },
    {"Stats",           CMD_TOGGLE_STATS,           true },
    {"Crosshair",       CMD_TOGGLE_CROSSHAIR,       true },
    {"Set Reference",   CMD_SET_REFERENCE,          false},
    {"Diff View",       CMD_TOGGLE_DIFF,            true },
    {"Diff Gain -",     CMD_DIFF_GAIN_DOWN,         false},
    {"Diff Gain +",     CMD_DIFF_GAIN_UP,           false},
    {"Pause",           CMD_TOGGLE_PAUSE,           true },
    {"Forward",         CMD_STEP_FORWARD,           false},
    {"Backward",        CMD_STEP_BACKWARD,          false},
    {"Recording",       CMD_TOGGLE_RECORDING,       true },
    {"Screenshot",      CMD_REQUEST_SCREENSHOT,     false},
    {"Flush Save",      CMD_FLUSH_SAVER,            false},
    {"Clear Selection", CMD_CLEAR_SELECTION,        false},
    {"Reset Zoom",      CMD_ZOOM_RESET,             false},
    {"Help",            CMD_SHOW_HELP,              false},
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

static void draw_icon(commands_e cmd, point_s center, float scale)
{
    int s           = (int)(ICON_SIZE * scale);
    fcolor_rgba_s c = UI_RGBA_WHITE;

    switch (cmd)
    {
        case CMD_TOGGLE_SMOOTHING:
            overlay_rect_draw((rect_s) {center.x - s / 2, center.y - s / 2, center.x, center.y}, c);
            overlay_rect_draw((rect_s) {center.x, center.y, center.x + s / 2, center.y + s / 2}, c);
            break;
        case CMD_TOGGLE_AUTO_LEVELS:
            overlay_rect_draw((rect_s) {center.x - s / 2, center.y + s / 2, center.x + s / 2, center.y + s / 2 + 2}, c);
            overlay_rect_draw((rect_s) {center.x - s / 4, center.y, center.x + s / 4, center.y + s / 2}, c);
            break;
        case CMD_TOGGLE_SATURATION:
            overlay_rect_draw((rect_s) {center.x - s / 4, center.y - s / 4, center.x + s / 4, center.y + s / 4}, c);
            break;
        case CMD_TOGGLE_HISTOGRAM:
        case CMD_TOGGLE_HISTOGRAM_SCALE:
        case CMD_TOGGLE_HISTOGRAM_CDF:
            overlay_rect_draw((rect_s) {center.x - s / 2, center.y, center.x - s / 4, center.y + s / 2}, c);
            overlay_rect_draw((rect_s) {center.x - s / 8, center.y - s / 4, center.x + s / 8, center.y + s / 2}, c);
            overlay_rect_draw((rect_s) {center.x + s / 4, center.y + s / 8, center.x + s / 2, center.y + s / 2}, c);
            break;
        case CMD_TOGGLE_GRADIENT:
            for (int i = 0; i < s; i += 3)
            {
                overlay_rect_draw((rect_s) {center.x - s / 2 + i, center.y + s / 2 - i, center.x - s / 2 + i + 2,
                                            center.y + s / 2 - i + 2},
                                  c);
            }
            break;
        case CMD_TOGGLE_INSPECTOR:
            overlay_rect_draw((rect_s) {center.x - s / 3, center.y - s / 6, center.x + s / 3, center.y + s / 6}, c);
            overlay_rect_draw((rect_s) {center.x - s / 6, center.y - s / 6, center.x + s / 6, center.y + s / 6},
                              UI_RGBA_BLACK);
            break;
        case CMD_TOGGLE_STATS:
            overlay_rect_draw((rect_s) {center.x - s / 2, center.y - s / 3, center.x + s / 2, center.y - s / 3 + 2}, c);
            overlay_rect_draw((rect_s) {center.x - s / 2, center.y + s / 3, center.x + s / 2, center.y + s / 3 + 2}, c);
            break;
        case CMD_TOGGLE_CROSSHAIR:
            overlay_rect_draw((rect_s) {center.x - s / 2, center.y - 1, center.x + s / 2, center.y + 1}, c);
            overlay_rect_draw((rect_s) {center.x - 1, center.y - s / 2, center.x + 1, center.y + s / 2}, c);
            break;
        case CMD_SET_REFERENCE:
            overlay_rect_outline_draw((rect_s) {center.x - s / 3, center.y - s / 3, center.x + s / 3, center.y + s / 3},
                                      c, 2);
            overlay_rect_draw((rect_s) {center.x - 2, center.y - 2, center.x + 2, center.y + 2}, c);
            break;
        case CMD_TOGGLE_DIFF:
            overlay_rect_outline_draw((rect_s) {center.x - s / 2, center.y - s / 2, center.x + s / 4, center.y + s / 4},
                                      c, 2);
            overlay_rect_outline_draw((rect_s) {center.x - s / 4, center.y - s / 4, center.x + s / 2, center.y + s / 2},
                                      c, 2);
            break;
        case CMD_DIFF_GAIN_DOWN:
            overlay_rect_draw((rect_s) {center.x - s / 2, center.y - 2, center.x + s / 2, center.y + 2}, c);
            break;
        case CMD_DIFF_GAIN_UP:
            overlay_rect_draw((rect_s) {center.x - s / 2, center.y - 2, center.x + s / 2, center.y + 2}, c);
            overlay_rect_draw((rect_s) {center.x - 2, center.y - s / 2, center.x + 2, center.y + s / 2}, c);
            break;
        case CMD_TOGGLE_PAUSE:
            overlay_rect_draw((rect_s) {center.x - s / 3, center.y - s / 3, center.x - s / 6, center.y + s / 3}, c);
            overlay_rect_draw((rect_s) {center.x + s / 6, center.y - s / 3, center.x + s / 3, center.y + s / 3}, c);
            break;
        case CMD_STEP_FORWARD:
            overlay_rect_draw((rect_s) {center.x - s / 3, center.y - s / 3, center.x + s / 3, center.y}, c);
            overlay_rect_draw((rect_s) {center.x - s / 3, center.y, center.x + s / 3, center.y + s / 3}, c);
            break;
        case CMD_STEP_BACKWARD:
            overlay_rect_draw((rect_s) {center.x + s / 3, center.y - s / 3, center.x - s / 3, center.y}, c);
            overlay_rect_draw((rect_s) {center.x + s / 3, center.y, center.x - s / 3, center.y + s / 3}, c);
            break;
        case CMD_TOGGLE_RECORDING:
            overlay_rect_outline_draw((rect_s) {center.x - s / 3, center.y - s / 3, center.x + s / 3, center.y + s / 3},
                                      c, 3);
            break;
        case CMD_REQUEST_SCREENSHOT:
            overlay_rect_outline_draw((rect_s) {center.x - s / 2, center.y - s / 3, center.x + s / 2, center.y + s / 3},
                                      c, 2);
            break;
        case CMD_FLUSH_SAVER:
            overlay_rect_draw((rect_s) {center.x - 2, center.y - s / 2, center.x + 2, center.y + s / 4}, c);
            overlay_rect_draw((rect_s) {center.x - s / 4, center.y, center.x + s / 4, center.y + 2}, c);
            break;
        case CMD_CLEAR_SELECTION:
            overlay_rect_outline_draw((rect_s) {center.x - s / 3, center.y - s / 3, center.x + s / 3, center.y + s / 3},
                                      c, 2);
            break;
        case CMD_ZOOM_RESET:
            overlay_rect_outline_draw((rect_s) {center.x - s / 3, center.y - s / 3, center.x + s / 6, center.y + s / 6},
                                      c, 2);
            overlay_rect_draw((rect_s) {center.x + s / 6, center.y + s / 6, center.x + s / 3, center.y + s / 3}, c);
            break;
        case CMD_SHOW_HELP:
            overlay_rect_draw((rect_s) {center.x - 2, center.y - s / 3, center.x + 2, center.y - s / 6}, c);
            overlay_rect_draw((rect_s) {center.x - 2, center.y - 2, center.x + 2, center.y + 2}, c);
            break;
        default:
            overlay_rect_outline_draw((rect_s) {center.x - s / 3, center.y - s / 3, center.x + s / 3, center.y + s / 3},
                                      c, 1);
            break;
    }
}

//----------------------------------------------- Отрисовка панели -----------------------------------------------------

static void get_panel_rect(image_size_s win, float scale, rect_s * out_panel, int * out_visible_count)
{
    int cols    = PANEL_COLS;
    int visible = 0;
    for (int i = 0; i < g_item_count; i++)
    {
        if (g_items[ i ].cmd != 0)
        {
            visible++;
        }
    }

    int rows = (visible + cols - 1) / cols;
    int pad  = (int)(PANEL_PADDING * scale);
    int w    = cols * (int)(CELL_SIZE * scale) + 2 * pad;
    int h    = rows * (int)(CELL_SIZE * scale) + 2 * pad;

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

    // Фон панели (обводку убрали)
    overlay_rect_draw(panel, UI_MENU_BG);

    int cols      = PANEL_COLS;
    int cell_w    = (int)(CELL_SIZE * scale);
    int cell_h    = (int)(CELL_SIZE * scale);
    int outer_pad = (int)(CELL_OUTER_PAD * scale);
    int inner_pad = (int)(CELL_INNER_PAD * scale);
    int panel_pad = (int)(PANEL_PADDING * scale);

    int start_x = panel.x0 + panel_pad;
    int start_y = panel.y0 + panel_pad;
    int idx     = 0;

    for (int i = 0; i < g_item_count; i++)
    {
        if (g_items[ i ].cmd == 0)
        {
            continue;
        }

        int row = idx / cols;
        int col = idx % cols;
        idx++;

        int cx = start_x + col * cell_w;
        int cy = start_y + row * cell_h;

        rect_s cell = {cx + outer_pad, cy + outer_pad, cx + cell_w - outer_pad, cy + cell_h - outer_pad};

        bool hovered = (g_hover.x >= cell.x0 && g_hover.x < cell.x1 && g_hover.y >= cell.y0 && g_hover.y < cell.y1);
        bool active  = g_items[ i ].is_toggle && cmd_is_active(g_items[ i ].cmd);

        // Фон ячейки
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
            overlay_rect_draw(cell, UI_CELL_BG);
        }
        overlay_rect_outline_draw(cell, UI_CELL_BORDER, 1);

        // Иконка — по центру верхней части ячейки
        int icon_s          = (int)(ICON_SIZE * scale);
        int icon_top        = cell.y0 + inner_pad;
        point_s icon_center = {cell.x0 + (cell.x1 - cell.x0) / 2, icon_top + icon_s / 2};
        draw_icon(g_items[ i ].cmd, icon_center, scale);

        // Текст — по центру нижней части
        const char * label = g_items[ i ].label;

        float fit_scale = TEXT_SCALE * scale;
        int tw          = overlay_text_measure(label, fit_scale);
        int max_w       = (cell.x1 - cell.x0) - 2 * inner_pad;
        while (tw > max_w && fit_scale > 0.45f)
        {
            fit_scale -= 0.05f;
            tw = overlay_text_measure(label, fit_scale);
        }

        int tx = cell.x0 + (cell.x1 - cell.x0 - tw) / 2;
        int ty = cell.y1 - inner_pad - (int)(16 * fit_scale);
        overlay_text_draw((point_s) {tx, ty}, label, UI_TEXT_PRIMARY, fit_scale);
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
        if (g_items[ i ].cmd == 0)
        {
            continue;
        }

        int row = idx / cols;
        int col = idx % cols;
        idx++;

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
