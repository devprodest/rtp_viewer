#include "progress_bar.h"

#include "render/overlay.h"
#include "render/ui/ui_color_scheme.h"
#include "render/window.h"
#include "util/utils.h"

#include <stdio.h>

#define PROGRESS_BAR_IDLE_TIMEOUT_US 3000000ULL // 3 секунды

static int g_current = 0;
static int g_total   = 0;

static point_s g_mouse           = {-1, -1};
static uint64_t g_last_motion_us = 0;
static bool g_has_motion         = false;

static void bar_rect(image_size_s win, float dpi_scale, rect_s * out_bar)
{
    const int bar_h  = (int)(6 * dpi_scale);
    const int margin = (int)(8 * dpi_scale);
    const int y      = win.y - bar_h - margin;
    out_bar->x0      = margin;
    out_bar->y0      = y;
    out_bar->x1      = win.x - margin;
    out_bar->y1      = y + bar_h;
}

void progress_bar_set(int current, int total)
{
    g_current = current;
    g_total   = total;
}

void progress_bar_notify_mouse(point_s mouse)
{
    image_size_s win  = window_get_framebuffer_size();
    const bool inside = (win.x > 0 && win.y > 0 &&          //
                         mouse.x >= 0 && mouse.x < win.x && //
                         mouse.y >= 0 && mouse.y < win.y);

    if (!inside)
    {
        // Запоминаем позицию, но таймер НЕ сбрасываем —
        // если мышь ушла за пределы окна, полоса должна угаснуть.
        g_mouse = mouse;
        return;
    }

    if (!g_has_motion || mouse.x != g_mouse.x || mouse.y != g_mouse.y)
    {
        g_last_motion_us = get_time_us();
        g_has_motion     = true;
    }
    g_mouse = mouse;
}

/// \brief Полоса видима, если мышь двигалась недавно ИЛИ курсор сейчас
///        над полосой. Иначе — скрыта после PROGRESS_BAR_IDLE_TIMEOUT_US.
static bool bar_visible(image_size_s win, float dpi_scale)
{
    if (!g_has_motion)
    {
        return true;
    }
    if (get_time_us() - g_last_motion_us <= PROGRESS_BAR_IDLE_TIMEOUT_US)
    {
        return true;
    }

    // Hover-исключение работает только пока мышь в пределах окна.
    const bool mouse_inside = g_mouse.x >= 0 && g_mouse.x < win.x && //
                              g_mouse.y >= 0 && g_mouse.y < win.y;
    if (!mouse_inside)
    {
        return false;
    }

    rect_s bar;
    bar_rect(win, dpi_scale, &bar);
    const int tol = (int)(6 * dpi_scale);
    return g_mouse.x >= bar.x0 && g_mouse.x < bar.x1 && //
           g_mouse.y >= bar.y0 - tol && g_mouse.y < bar.y1 + tol;
}

bool progress_bar_hit_test(point_s screen, float dpi_scale, float * out_fraction)
{
    if (out_fraction)
    {
        *out_fraction = 0.0f;
    }
    if (g_total <= 0)
    {
        return false;
    }

    image_size_s win = window_get_framebuffer_size();
    if (win.x <= 0 || win.y <= 0)
    {
        return false;
    }
    if (!bar_visible(win, dpi_scale))
    {
        return false;
    }

    rect_s bar;
    bar_rect(win, dpi_scale, &bar);

    const int tol = (int)(6 * dpi_scale);

    if (out_fraction)
    {
        float frac = (float)(screen.x - bar.x0) / (float)(bar.x1 - bar.x0);
        if (frac < 0.0f)
        {
            frac = 0.0f;
        }
        if (frac > 1.0f)
        {
            frac = 1.0f;
        }
        *out_fraction = frac;
    }

    return screen.y >= bar.y0 - tol && screen.y < bar.y1 + tol;
}

void progress_bar_draw(image_size_s win, float dpi_scale)
{
    if (g_total <= 0)
    {
        return;
    }
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }
    if (!bar_visible(win, dpi_scale))
    {
        return;
    }

    rect_s bar;
    bar_rect(win, dpi_scale, &bar);

    // Текст-счётчик
    char buf[ 64 ];
    snprintf(buf, sizeof(buf), "%d / %d", g_current + 1, g_total);

    const float text_scale = dpi_scale * 1.f;
    const int tw           = overlay_text_measure(buf, text_scale);
    const int tx           = (win.x - tw) / 2;
    int ty                 = bar.y0 - (int)(16 * text_scale);
    if (ty < 0)
    {
        ty = 0;
    }

    // Подложка: охватывает текст и полосу с отступами по краям.
    const int pad_y = (int)(6 * dpi_scale);
    rect_s panel    = {
        .x0 = 0,
        .y0 = ty - pad_y,
        .x1 = win.x,
        .y1 = win.y,
    };
    overlay_rect_draw(panel, UI_PROGRESS_PANEL_BG);

    // Полоса поверх панели
    overlay_rect_draw(bar, UI_PROGRESS_BG);

    float frac = (float)g_current / (float)(g_total - 1);
    if (frac < 0.0f)
    {
        frac = 0.0f;
    }
    if (frac > 1.0f)
    {
        frac = 1.0f;
    }

    int fill_x  = bar.x0 + (int)((float)(bar.x1 - bar.x0) * frac);
    rect_s fill = {bar.x0, bar.y0, fill_x, bar.y1};
    overlay_rect_draw(fill, UI_PROGRESS_FILL);

    overlay_rect_outline_draw(bar, UI_PROGRESS_BORDER, 1);

    // Текст поверх панели
    overlay_text_draw((point_s) {tx, ty}, buf, UI_TEXT_PRIMARY, text_scale);
}
