#include "context_menu.h"

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

#define CMD_GAP        ((commands_e) - 1)

static const commands_e g_menu_order[] = {
    CMD_TOGGLE_GRADIENT,
    CMD_TOGGLE_INSPECTOR,
    CMD_TOGGLE_SATURATION,
    CMD_TOGGLE_HISTOGRAM,
    CMD_TOGGLE_HISTOGRAM_SCALE,
    CMD_TOGGLE_STATS,

    CMD_TOGGLE_LINE_PROFILE,
    CMD_TOGGLE_VECTORSCOPE,
    CMD_SET_REFERENCE,
    CMD_TOGGLE_DIFF,
    CMD_DIFF_GAIN_DOWN,
    CMD_DIFF_GAIN_UP,

    CMD_STEP_BACKWARD,
    CMD_TOGGLE_PAUSE,
    CMD_STEP_FORWARD,
    CMD_TOGGLE_RECORDING,
    CMD_FLUSH_SAVER,
    CMD_REQUEST_SCREENSHOT,

    CMD_TOGGLE_AUTO_LEVELS,
    CMD_TOGGLE_SMOOTHING,
    CMD_CLEAR_SELECTION,
    CMD_ZOOM_RESET,
    CMD_TOGGLE_FULLSCREEN,
    CMD_SHOW_HELP,
};
#define MENU_ORDER_COUNT ((int)(sizeof(g_menu_order) / sizeof(g_menu_order[ 0 ])))

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

static void get_panel_rect(image_size_s win, float scale, rect_s * out_panel)
{
    int visible = MENU_ORDER_COUNT;
    int cols    = PANEL_COLS;
    int rows    = (visible + cols - 1) / cols;
    int pad     = (int)(PANEL_PADDING * scale);
    int tip_h   = (int)(TOOLTIP_HEIGHT * scale);

    int w = cols * (int)(CELL_SIZE * scale) + 2 * pad;
    int h = rows * (int)(CELL_SIZE * scale) + 2 * pad + tip_h;

    int x = (win.x - w) / 2;
    int y = (win.y - h) / 2;

    out_panel->x0 = x;
    out_panel->y0 = y;
    out_panel->x1 = x + w;
    out_panel->y1 = y + h;
}

void context_menu_draw(image_size_s win, float scale)
{
    if (!g_open)
    {
        return;
    }

    rect_s panel;
    get_panel_rect(win, scale, &panel);

    int cols = PANEL_COLS, cell_w = (int)(CELL_SIZE * scale), cell_h = (int)(CELL_SIZE * scale);
    int outer_pad = (int)(CELL_OUTER_PAD * scale), inner_pad = (int)(CELL_INNER_PAD * scale);
    int panel_pad = (int)(PANEL_PADDING * scale);
    int start_x = panel.x0 + panel_pad, start_y = panel.y0 + panel_pad;
    int idx = 0, hovered_cmd = -1;

    // ----- ПРОХОД 1: Overlay (фон, кнопки, текст) -----
    overlay_begin(win);
    overlay_rect_draw(panel, UI_MENU_BG);

    for (int i = 0; i < MENU_ORDER_COUNT; i++)
    {
        int row = idx / cols, col = idx % cols;
        idx++;
        commands_e cmd = g_menu_order[ i ];
        if (cmd == CMD_GAP)
        {
            continue;
        }

        const command_info_s * info = cmd_get_info(cmd);
        if (!info || info->type == CMD_TYPE_NONE)
        {
            continue;
        }

        int cx = start_x + col * cell_w, cy = start_y + row * cell_h;
        rect_s cell = {cx + outer_pad, cy + outer_pad, cx + cell_w - outer_pad, cy + cell_h - outer_pad};

        bool hovered = (g_hover.x >= cell.x0 && g_hover.x < cell.x1 && g_hover.y >= cell.y0 && g_hover.y < cell.y1);
        bool active  = (info->type == CMD_TYPE_TOGGLE) && cmd_is_active(cmd);
        if (hovered)
        {
            hovered_cmd = (int)cmd;
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
            overlay_rect_draw(cell, group_bg_color(info->group));
        }
        overlay_rect_outline_draw(cell, UI_CELL_BORDER, 1);

        if (!svg_icon_exists(cmd))
        {
            point_s icon_center = {cx + cell_w / 2, cy + cell_h / 2 - 8};
            draw_icon(icon_center, scale);
        }

        const char * label = info->label;
        float fit_scale    = TEXT_SCALE * scale;
        int tw             = overlay_text_measure(label, fit_scale);
        int max_w          = (cell.x1 - cell.x0) - 2 * inner_pad;
        while (tw > max_w && fit_scale > 0.45f)
        {
            fit_scale -= 0.05f;
            tw = overlay_text_measure(label, fit_scale);
        }

        overlay_text_draw(
            (point_s) {cell.x0 + (cell.x1 - cell.x0 - tw) / 2, cell.y1 - inner_pad - (int)(16 * fit_scale)}, label,
            UI_TEXT_PRIMARY, fit_scale);
    }

    if (hovered_cmd >= 0)
    {
        const command_info_s * info = cmd_get_info((commands_e)hovered_cmd);
        int tip_h                   = (int)(TOOLTIP_HEIGHT * scale);
        rect_s tooltip_area         = {panel.x0, panel.y1 - tip_h, panel.x1, panel.y1};
        char tip_buf[ 128 ];

        if (info->hotkey_str && info->hotkey_str[ 0 ] != '\0')
        {
            snprintf(tip_buf, sizeof(tip_buf), "%s  [%s]", info->label, info->hotkey_str);
        }
        else
        {
            snprintf(tip_buf, sizeof(tip_buf), "%s", info->label);
        }

        float tip_scale = 0.85f * scale;
        int tw          = overlay_text_measure(tip_buf, tip_scale);
        int tx          = tooltip_area.x0 + (tooltip_area.x1 - tooltip_area.x0 - tw) / 2;
        int ty          = tooltip_area.y0 + (tooltip_area.y1 - tooltip_area.y0 - (int)(14 * tip_scale)) / 2;
        overlay_text_draw((point_s) {tx, ty}, tip_buf, UI_TEXT_PRIMARY, tip_scale);
    }
    overlay_end();

    // ----- ПРОХОД 2: NanoVG (иконки) -----
    svg_icons_begin(win, scale);
    {
        int i_idx = 0;
        for (int i = 0; i < MENU_ORDER_COUNT; i++)
        {
            int row = i_idx / cols, col = i_idx % cols;
            i_idx++;
            commands_e cmd = g_menu_order[ i ];
            if (cmd == CMD_GAP)
            {
                continue;
            }

            const command_info_s * info = cmd_get_info(cmd);
            if (!info || info->type == CMD_TYPE_NONE)
            {
                continue;
            }

            int cx = start_x + col * cell_w, cy = start_y + row * cell_h;
            int icon_s = (int)(ICON_SIZE * scale), icon_top = cy + (int)(CELL_OUTER_PAD * scale) + inner_pad;
            svg_icons_draw(cmd, (point_s) {cx + cell_w / 2, icon_top + icon_s / 2}, (float)icon_s, UI_RGBA_WHITE);
        }
    }
    svg_icons_end();
}

bool context_menu_handle_click(point_s mouse, image_size_s win, float scale)
{
    if (!g_open)
    {
        return false;
    }

    rect_s panel;
    get_panel_rect(win, scale, &panel);

    if (mouse.x < panel.x0 || mouse.x >= panel.x1 || mouse.y < panel.y0 || mouse.y >= panel.y1)
    {
        context_menu_close();
        return true;
    }

    int tip_h = (int)(TOOLTIP_HEIGHT * scale);
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

    for (int i = 0; i < MENU_ORDER_COUNT; i++)
    {
        int row = idx / cols;
        int col = idx % cols;
        idx++;

        if (g_menu_order[ i ] == CMD_GAP)
        {
            continue;
        }

        int cx = start_x + col * cell_w;
        int cy = start_y + row * cell_h;

        rect_s cell = {cx + outer_pad, cy + outer_pad, cx + cell_w - outer_pad, cy + cell_h - outer_pad};

        if (mouse.x >= cell.x0 && mouse.x < cell.x1 && mouse.y >= cell.y0 && mouse.y < cell.y1)
        {
            cmd_execute(g_menu_order[ i ]);
            context_menu_close();
            return true;
        }
    }

    context_menu_close();
    return true;
}
