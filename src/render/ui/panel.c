#include "panel.h"

#include "app/settings.h"
#include "log/logger.h"
#include "render/overlay.h"
#include "render/ui/ui_color_scheme.h"
#include "util/utils.h"

#include <stdio.h>

#define MAX_PANELS   8
#define SNAP_PX_BASE 16

static const panel_s * g_panels[ MAX_PANELS ];
static int g_count = 0;

static int g_drag_idx        = -1;
static point_s g_drag_offset = {0, 0};
static bool g_was_left       = false;

//------------------------------------------------------- API ---------------------------------------------------------

int panel_title_height(float dpi_scale)
{
    return (int)(PANEL_TITLE_H_BASE * dpi_scale);
}

void panel_register(const panel_s * p)
{
    if (g_count >= MAX_PANELS)
    {
        return;
    }
    g_panels[ g_count++ ] = p;
}

void panel_unregister_all(void)
{
    g_count    = 0;
    g_drag_idx = -1;
}

//------------------------------------------------------- Геометрия ----------------------------------------------------

static image_size_s panel_full_size(const panel_s * p, float dpi_scale)
{
    image_size_s content = p->get_size(dpi_scale);
    int title_h          = panel_title_height(dpi_scale);
    return (image_size_s) {content.x, content.y + title_h};
}

static rect_s panel_title_rect(const panel_s * p, float dpi_scale)
{
    point_s pos          = p->get_pos();
    image_size_s content = p->get_size(dpi_scale);
    int title_h          = panel_title_height(dpi_scale);
    return (rect_s) {pos.x, pos.y, pos.x + content.x, pos.y + title_h};
}

static rect_s panel_close_rect(const panel_s * p, float dpi_scale)
{
    rect_s title = panel_title_rect(p, dpi_scale);
    int size     = title.y1 - title.y0 - 6;
    return (rect_s) {title.x1 - size - 3, title.y0 + 3, title.x1 - 3, title.y1 - 3};
}

static bool point_in_rect(point_s pt, rect_s r)
{
    return pt.x >= r.x0 && pt.x < r.x1 && pt.y >= r.y0 && pt.y < r.y1;
}

//------------------------------------------------------- Snap ---------------------------------------------------------

static void apply_snap(point_s * pos, image_size_s full_size, int drag_idx, image_size_s win, float dpi_scale)
{
    const int snap = (int)(SNAP_PX_BASE * dpi_scale);

    // Snap к краям окна
    if (pos->x < snap)
    {
        pos->x = 0;
    }
    if (pos->x + full_size.x > win.x - snap)
    {
        pos->x = win.x - full_size.x;
    }
    if (pos->y < snap)
    {
        pos->y = 0;
    }
    if (pos->y + full_size.y > win.y - snap)
    {
        pos->y = win.y - full_size.y;
    }

    // Snap к другим видимым панелям (по вертикали/горизонтали)
    for (int i = 0; i < g_count; i++)
    {
        if (i == drag_idx)
        {
            continue;
        }
        const panel_s * q = g_panels[ i ];
        if (!q->is_visible())
        {
            continue;
        }

        point_s q_pos       = q->get_pos();
        image_size_s q_full = panel_full_size(q, dpi_scale);

        // Левые/правые края выровнены
        if (utils_abs(pos->x - q_pos.x) < snap)
        {
            pos->x = q_pos.x;
        }
        if (utils_abs((pos->x + full_size.x) - (q_pos.x + q_full.x)) < snap)
        {
            pos->x = q_pos.x + q_full.x - full_size.x;
        }

        // Верхние/нижние края выровнены
        if (utils_abs(pos->y - q_pos.y) < snap)
        {
            pos->y = q_pos.y;
        }
        if (utils_abs((pos->y + full_size.y) - (q_pos.y + q_full.y)) < snap)
        {
            pos->y = q_pos.y + q_full.y - full_size.y;
        }
    }

    // Финальный clamp, чтобы не вылететь за окно
    pos->x = utils_clamp(pos->x, 0, utils_max(0, win.x - full_size.x));
    pos->y = utils_clamp(pos->y, 0, utils_max(0, win.y - full_size.y));
}

//------------------------------------------------------- Input -------------------------------------------------------

bool panel_handle_mouse(point_s mouse, bool left_down, bool menu_open, image_size_s win, float dpi_scale)
{
    // Если меню открыто — панели не реагируют
    if (menu_open)
    {
        g_drag_idx = -1;
        g_was_left = left_down;
        return false;
    }

    bool consumed = false;

    // Начало drag
    if (left_down && !g_was_left)
    {
        // Сначала проверяем крестик закрытия (он приоритетнее)
        for (int i = 0; i < g_count; i++)
        {
            const panel_s * p = g_panels[ i ];
            if (!p->is_visible())
            {
                continue;
            }

            if (point_in_rect(mouse, panel_close_rect(p, dpi_scale)))
            {
                p->on_toggle();
                settings_set_panel_enabled(p->id, p->is_visible());
                consumed   = true;
                g_drag_idx = -1;
                g_was_left = left_down;
                return consumed;
            }
        }

        // Потом — title bar (начало drag)
        for (int i = 0; i < g_count; i++)
        {
            const panel_s * p = g_panels[ i ];
            if (!p->is_visible())
            {
                continue;
            }

            if (point_in_rect(mouse, panel_title_rect(p, dpi_scale)))
            {
                point_s pos   = p->get_pos();
                g_drag_offset = (point_s) {mouse.x - pos.x, mouse.y - pos.y};
                g_drag_idx    = i;
                consumed      = true;
                break;
            }
        }
    }
    // Обновление drag
    else if (left_down && g_drag_idx >= 0)
    {
        const panel_s * p = g_panels[ g_drag_idx ];
        image_size_s full = panel_full_size(p, dpi_scale);
        point_s new_pos   = {mouse.x - g_drag_offset.x, mouse.y - g_drag_offset.y};

        apply_snap(&new_pos, full, g_drag_idx, win, dpi_scale);

        p->set_pos(new_pos);
        settings_set_panel_pos(p->id, new_pos);
        consumed = true;
    }
    // Конец drag
    else if (!left_down && g_was_left && g_drag_idx >= 0)
    {
        const panel_s * p = g_panels[ g_drag_idx ];
        settings_set_panel_pos(p->id, p->get_pos());
        g_drag_idx = -1;
        consumed   = true;
    }

    g_was_left = left_down;
    return consumed;
}

//------------------------------------------------------- Chrome -------------------------------------------------------

static void draw_close_icon(rect_s r, fcolor_rgb_s color, float dpi_scale)
{
    int cx = (r.x0 + r.x1) / 2;
    int cy = (r.y0 + r.y1) / 2;
    int s  = (int)(4 * dpi_scale);
    int th = utils_max(1, (int)(1 * dpi_scale));

    // Две диагонали как ступенчатые прямоугольники
    for (int i = -s; i <= s; i++)
    {
        fcolor_rgba_s c = UI_RGBA_FROM_RGB(color, 1.0f);
        overlay_rect_draw((rect_s) {cx + i, cy + i, cx + i + th, cy + i + th}, c);
        overlay_rect_draw((rect_s) {cx + i, cy - i, cx + i + th, cy - i + th}, c);
    }
}

void panel_draw_chrome(image_size_s win, float dpi_scale)
{
    (void)win;
    int title_h = panel_title_height(dpi_scale);

    for (int i = 0; i < g_count; i++)
    {
        const panel_s * p = g_panels[ i ];
        if (!p->is_visible())
        {
            continue;
        }

        point_s pos          = p->get_pos();
        image_size_s content = p->get_size(dpi_scale);

        // Внешняя рамка всей панели
        rect_s outer = {pos.x, pos.y, pos.x + content.x, pos.y + title_h + content.y};
        overlay_rect_outline_draw(outer, UI_PANEL_BORDER, 1);

        // Title bar
        rect_s title_rect = {pos.x, pos.y, pos.x + content.x, pos.y + title_h};
        overlay_rect_draw(title_rect, UI_PANEL_BG);

        // Заголовок
        float ts = 0.9f * dpi_scale;
        int tx   = pos.x + (int)(8 * dpi_scale);
        int ty   = pos.y + (title_h - (int)(14 * ts)) / 2;
        overlay_text_draw((point_s) {tx, ty}, p->title, UI_TEXT_PRIMARY, ts);

        // Крестик закрытия
        rect_s close_r = panel_close_rect(p, dpi_scale);
        overlay_rect_draw(close_r, UI_PANEL_BG_STRONG);
        draw_close_icon(close_r, UI_TEXT_PRIMARY, dpi_scale);
    }
}

//------------------------------------------------------- Прочее -------------------------------------------------------

void panel_clamp_to_window(image_size_s win, float dpi_scale)
{
    for (int i = 0; i < g_count; i++)
    {
        const panel_s * p = g_panels[ i ];
        if (!p->is_visible())
        {
            continue;
        }

        point_s pos       = p->get_pos();
        image_size_s full = panel_full_size(p, dpi_scale);

        int new_x = utils_clamp(pos.x, 0, utils_max(0, win.x - full.x));
        int new_y = utils_clamp(pos.y, 0, utils_max(0, win.y - full.y));

        if (new_x != pos.x || new_y != pos.y)
        {
            p->set_pos((point_s) {new_x, new_y});
        }
    }
}

void panel_apply_settings(void)
{
    for (int i = 0; i < g_count; i++)
    {
        const panel_s * p = g_panels[ i ];

        point_s pos;
        if (settings_get_panel_pos(p->id, &pos))
        {
            p->set_pos(pos);
        }

        bool en;
        if (settings_get_panel_enabled(p->id, &en))
        {
            if (en != p->is_visible())
            {
                p->on_toggle();
            }
        }
    }
}

void panel_save_settings(void)
{
    for (int i = 0; i < g_count; i++)
    {
        const panel_s * p = g_panels[ i ];
        settings_set_panel_pos(p->id, p->get_pos());
        settings_set_panel_enabled(p->id, p->is_visible());
    }
}
