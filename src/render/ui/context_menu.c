#include "context_menu.h"

#include "control/commands.h"
#include "render/overlay.h"

#include <stddef.h>

#define MENU_WIDTH  200
#define ITEM_HEIGHT 26
#define PADDING     6

typedef struct context_menu_item_s
{
    const char * label;
    commands_e cmd;
    bool is_toggle;
} context_menu_item_s;

static const context_menu_item_s g_items[] = {
    {"Smoothing",       CMD_TOGGLE_SMOOTHING,       true },
    {"Saturation",      CMD_TOGGLE_SATURATION,      true },
    {"Histogram",       CMD_TOGGLE_HISTOGRAM,       true },
    {"Log Scale",       CMD_TOGGLE_HISTOGRAM_SCALE, true },
    {"CDF",             CMD_TOGGLE_HISTOGRAM_CDF,   true },
    {"Gradient",        CMD_TOGGLE_GRADIENT,        true },
    {"Inspector",       CMD_TOGGLE_INSPECTOR,       true },
    {"Stats",           CMD_TOGGLE_STATS,           true },
    {"Crosshair",       CMD_TOGGLE_CROSSHAIR,       true },
    {"Pause",           CMD_TOGGLE_PAUSE,           true },
    {"Recording",       CMD_TOGGLE_RECORDING,       true },
    {"---",             0,                          false}, // separator
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

void context_menu_draw(image_size_s win, float scale)
{
    if (!g_open)
    {
        return;
    }

    int w   = (int)(MENU_WIDTH * scale);
    int h   = (int)(g_item_count * ITEM_HEIGHT * scale);
    int pad = (int)(PADDING * scale);

    int x = g_pos.x;
    int y = g_pos.y;
    if (x + w > win.x)
    {
        x = win.x - w;
    }
    if (y + h > win.y)
    {
        y = win.y - h;
    }
    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }

    rect_s box = {x, y, x + w, y + h};
    overlay_rect_draw(box, (fcolor_rgba_s) {0.15f, 0.15f, 0.15f, 0.95f});
    overlay_rect_outline_draw(box, (fcolor_rgba_s) {0.5f, 0.5f, 0.5f, 1.0f}, 1);

    int item_h = (int)(ITEM_HEIGHT * scale);
    for (int i = 0; i < g_item_count; i++)
    {
        int iy           = y + i * item_h;
        rect_s item_rect = {x, iy, x + w, iy + item_h};

        if (g_items[ i ].label[ 0 ] == '-') // separator
        {
            overlay_rect_draw((rect_s) {x + pad, iy + item_h / 2, x + w - pad, iy + item_h / 2 + 1},
                              (fcolor_rgba_s) {0.4f, 0.4f, 0.4f, 1.0f});
            continue;
        }

        bool hovered = (g_hover.x >= item_rect.x0 && g_hover.x < item_rect.x1 && g_hover.y >= item_rect.y0 &&
                        g_hover.y < item_rect.y1);

        bool active = g_items[ i ].is_toggle && cmd_is_active(g_items[ i ].cmd);

        if (active)
        {
            fcolor_rgba_s bg =
                hovered ? (fcolor_rgba_s) {0.30f, 0.70f, 0.30f, 1.0f} : (fcolor_rgba_s) {0.20f, 0.50f, 0.20f, 1.0f};
            overlay_rect_draw(item_rect, bg);
        }
        else if (hovered)
        {
            overlay_rect_draw(item_rect, (fcolor_rgba_s) {0.3f, 0.35f, 0.45f, 1.0f});
        }

        overlay_text_draw((point_s) {x + pad, iy + (int)(4 * scale)}, g_items[ i ].label,
                          (fcolor_rgb_s) {1.0f, 1.0f, 1.0f}, 1.f * scale);
    }
}

bool context_menu_handle_click(point_s mouse, image_size_s win, float scale)
{
    if (!g_open)
    {
        return false;
    }

    int w = (int)(MENU_WIDTH * scale);
    int h = (int)(g_item_count * ITEM_HEIGHT * scale);
    int x = g_pos.x;
    int y = g_pos.y;
    if (x + w > win.x)
    {
        x = win.x - w;
    }
    if (y + h > win.y)
    {
        y = win.y - h;
    }
    if (x < 0)
    {
        x = 0;
    }
    if (y < 0)
    {
        y = 0;
    }

    if (mouse.x < x || mouse.x >= x + w || mouse.y < y || mouse.y >= y + h)
    {
        context_menu_close();
        return true; // клик был "пойман", меню закрылось, но команду не выполняем
    }

    int item_h = (int)(ITEM_HEIGHT * scale);
    int index  = (mouse.y - y) / item_h;
    if (index >= 0 && index < g_item_count)
    {
        if (g_items[ index ].label[ 0 ] != '-')
        {
            cmd_execute(g_items[ index ].cmd);
        }
    }
    context_menu_close();
    return true;
}
