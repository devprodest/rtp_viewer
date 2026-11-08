#include "panel.h"

#include "app/settings.h"
#include "render/overlay.h"
#include "render/ui/ui_color_scheme.h"
#include "util/utils.h"

#include <string.h>

#define MAX_PANELS   8
#define SNAP_PX_BASE 16

//----------------------------------------------------------------------------------------------------------------------

typedef enum
{
    PANEL_ANCHOR_LEFT  = 0,
    PANEL_ANCHOR_RIGHT = 1,
} panel_anchor_h_e;

typedef enum
{
    PANEL_ANCHOR_TOP    = 0,
    PANEL_ANCHOR_BOTTOM = 1,
} panel_anchor_v_e;

typedef struct
{
    panel_anchor_h_e h;
    panel_anchor_v_e v;
} panel_anchor_t;


//----------------------------------------------------------------------------------------------------------------------

static int g_drag_idx          = -1;
static image_size_s g_last_win = {0, 0};
static point_s g_drag_offset   = {0, 0};
static bool g_was_left         = false;

static int g_count = 0;
static const panel_s * g_panels[ MAX_PANELS ];
static panel_anchor_t g_anchors[ MAX_PANELS ];

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
    memset(g_anchors, 0, sizeof(g_anchors));
    g_last_win = (image_size_s) {0, 0};
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

        // Обновляем anchor в реальном времени: как только панель после snap
        // прилипла к краю окна — переключаем anchor туда, чтобы подсветка
        // в panel_draw_chrome перемещалась вместе с панелью.
        if (new_pos.x <= 0)
        {
            g_anchors[ g_drag_idx ].h = PANEL_ANCHOR_LEFT;
        }
        else if (new_pos.x + full.x >= win.x)
        {
            g_anchors[ g_drag_idx ].h = PANEL_ANCHOR_RIGHT;
        }

        if (new_pos.y <= 0)
        {
            g_anchors[ g_drag_idx ].v = PANEL_ANCHOR_TOP;
        }
        else if (new_pos.y + full.y >= win.y)
        {
            g_anchors[ g_drag_idx ].v = PANEL_ANCHOR_BOTTOM;
        }

        p->set_pos(new_pos);
        settings_set_panel_pos(p->id, new_pos);
        consumed = true;
    }
    // Конец drag
    else if (!left_down && g_was_left && g_drag_idx >= 0)
    {
        const panel_s * p = g_panels[ g_drag_idx ];
        image_size_s full = panel_full_size(p, dpi_scale);
        point_s pos       = p->get_pos();

        const int snap = (int)(SNAP_PX_BASE * dpi_scale);

        // Обновляем anchor только если панель реально у края
        int anchor_h = g_anchors[ g_drag_idx ].h;
        int anchor_v = g_anchors[ g_drag_idx ].v;

        if (pos.x <= snap)
        {
            anchor_h = PANEL_ANCHOR_LEFT;
        }
        else if (pos.x + full.x >= win.x - snap)
        {
            anchor_h = PANEL_ANCHOR_RIGHT;
        }

        if (pos.y <= snap)
        {
            anchor_v = PANEL_ANCHOR_TOP;
        }
        else if (pos.y + full.y >= win.y - snap)
        {
            anchor_v = PANEL_ANCHOR_BOTTOM;
        }

        g_anchors[ g_drag_idx ].h = (panel_anchor_h_e)anchor_h;
        g_anchors[ g_drag_idx ].v = (panel_anchor_v_e)anchor_v;

        settings_set_panel_pos(p->id, pos);
        settings_set_panel_anchor(p->id, anchor_h, anchor_v);
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

/// \brief Рисует L-образный маркер якоря в углу панели.
/// \details Угол выбирается по anchor_h/anchor_v; размер в пикселях
///          масштабируется dpi. Тёмная окантовка — чтобы маркер был
///          заметен на любом контенте панели.
static void draw_anchor_corner(rect_s panel, int i, float dpi_scale)
{
    if (i != g_drag_idx)
    {
        return;
    }

    panel_anchor_h_e h = g_anchors[ i ].h;
    panel_anchor_v_e v = g_anchors[ i ].v;


    const int size = utils_max(12, (int)(18 * dpi_scale));
    const int th   = utils_max(3, (int)(4 * dpi_scale));

    const fcolor_rgba_s accent = UI_COLOR_ACCENT_ACTIVE_HOVER;

    // Координаты угла, к которому привязана панель
    const int protrude = (int)(2 * dpi_scale);
    const int cx       = (h == PANEL_ANCHOR_LEFT) ? panel.x0 - protrude : panel.x1 + protrude;
    const int cy       = (v == PANEL_ANCHOR_TOP) ? panel.y0 - protrude : panel.y1 + protrude;

    // Направление «внутрь панели» от угла
    const int sx = (h == PANEL_ANCHOR_LEFT) ? 1 : -1;
    const int sy = (v == PANEL_ANCHOR_TOP) ? 1 : -1;

    // Горизонтальный сегмент L
    int hx0 = cx;
    int hx1 = cx + sx * size;
    int hy0 = cy;
    int hy1 = cy + sy * th;
    if (hx0 > hx1)
    {
        int t = hx0;
        hx0   = hx1;
        hx1   = t;
    }
    if (hy0 > hy1)
    {
        int t = hy0;
        hy0   = hy1;
        hy1   = t;
    }

    // Вертикальный сегмент L
    int vx0 = cx;
    int vx1 = cx + sx * th;
    int vy0 = cy;
    int vy1 = cy + sy * size;
    if (vx0 > vx1)
    {
        int t = vx0;
        vx0   = vx1;
        vx1   = t;
    }
    if (vy0 > vy1)
    {
        int t = vy0;
        vy0   = vy1;
        vy1   = t;
    }
    // Акцентный L
    overlay_rect_draw((rect_s) {hx0, hy0, hx1, hy1}, accent);
    overlay_rect_draw((rect_s) {vx0, vy0, vx1, vy1}, accent);
}

void panel_draw_chrome(image_size_s, float dpi_scale)
{
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

        // Подсветка якоря во время перетаскивания
        draw_anchor_corner(outer, i, dpi_scale);
    }
}

//------------------------------------------------------- Прочее -------------------------------------------------------

void panel_clamp_to_window(image_size_s win, float dpi_scale)
{
    const bool resized = (win.x != g_last_win.x || win.y != g_last_win.y);

    for (int i = 0; i < g_count; i++)
    {
        const panel_s * p = g_panels[ i ];
        if (!p->is_visible())
        {
            continue;
        }
        if (i == g_drag_idx)
        {
            continue; // не трогать панель во время drag
        }

        point_s pos       = p->get_pos();
        image_size_s full = panel_full_size(p, dpi_scale);

        int new_x = pos.x;
        int new_y = pos.y;

        if (resized)
        {
            // Для LEFT/TOP: абсолютная координата и так отсчитывается от
            // левого/верхнего края, поэтому расстояние до края сохраняется
            // автоматически. Ничего не делаем.
            //
            // Для RIGHT/BOTTOM: панель привязана к правому/нижнему краю,
            // сдвигаем на дельту размера окна, чтобы расстояние до края
            // осталось тем же.
            if (g_anchors[ i ].h == PANEL_ANCHOR_RIGHT)
            {
                new_x = pos.x + (win.x - g_last_win.x);
            }
            if (g_anchors[ i ].v == PANEL_ANCHOR_BOTTOM)
            {
                new_y = pos.y + (win.y - g_last_win.y);
            }
        }

        // Clamp на случай, если окно уменьшилось и панель вылезла
        new_x = utils_clamp(new_x, 0, utils_max(0, win.x - full.x));
        new_y = utils_clamp(new_y, 0, utils_max(0, win.y - full.y));

        if (new_x != pos.x || new_y != pos.y)
        {
            p->set_pos((point_s) {new_x, new_y});
        }
    }

    g_last_win = win;
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

        int h = 0, v = 0;
        settings_get_panel_anchor(p->id, &h, &v);
        g_anchors[ i ].h = (panel_anchor_h_e)h;
        g_anchors[ i ].v = (panel_anchor_v_e)v;

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
