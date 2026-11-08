#include "line_profile_view.h"

#include "analysis/frame_context.h"
#include "render/overlay.h"
#include "render/ui/pixel_probe.h"
#include "render/ui/ui_color_scheme.h"
#include "render/viewport.h"
#include "util/utils.h"

//------------------------------------------------------- Параметры ----------------------------------------------------

/// \brief Толщина полосы профиля в пикселях (до dpi-скейла).
#define PANEL_THICKNESS 64

/// \brief Толщина тёмной подложки под баром (в пикселях).
#define BAR_OUTLINE     2

static bool g_enabled = false;

//------------------------------------------------------- API ---------------------------------------------------------

void line_profile_view_toggle(void)
{
    g_enabled = !g_enabled;
}

bool line_profile_view_is_enabled(void)
{
    return g_enabled;
}

//------------------------------------------------------- Помощники ---------------------------------------------------

/// \brief Возвращает нормализованное значение пикселя в [0..1].
static float pixel_norm(frame_pixel_t px, int bpp, float max_val)
{
    float v = (bpp <= 2) ? (float)px.raw : (float)px.luma;
    return utils_clamp(v / max_val, 0.0f, 1.0f);
}

//------------------------------------------------------- Отрисовка ---------------------------------------------------

void line_profile_view_draw(image_size_s win, float dpi_scale)
{
    if (!g_enabled)
    {
        return;
    }
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }
    if (!frame_context_is_valid())
    {
        return;
    }

    // Получаем позицию курсора и переводим в координаты кадра
    point_s mouse = pixel_probe_get_mouse();
    point_s cursor_frame;
    if (!viewport_screen_to_frame(mouse, &cursor_frame))
    {
        return;
    }

    const image_size_s fsize = frame_context_size();
    if (cursor_frame.x < 0 || cursor_frame.x >= fsize.x)
    {
        return;
    }
    if (cursor_frame.y < 0 || cursor_frame.y >= fsize.y)
    {
        return;
    }

    const int bpp       = frame_context_bpp();
    const int thickness = (int)(PANEL_THICKNESS * dpi_scale);
    const int outline   = utils_max(1, (int)(BAR_OUTLINE * dpi_scale));
    const float max_val = (bpp == 2) ? 65535.0f : 255.0f;

    //-------------------------------------------------------
    // Левая полоса: вертикальный профиль (срез по столбцу X=cursor)
    //-------------------------------------------------------
    for (int sy = 0; sy < win.y; sy++)
    {
        point_s f;
        if (!viewport_screen_to_frame((point_s) {mouse.x, sy}, &f))
        {
            continue;
        }
        if (f.y < 0 || f.y >= fsize.y)
        {
            continue;
        }

        frame_pixel_t px = frame_context_read(cursor_frame.x, f.y);
        float norm       = pixel_norm(px, bpp, max_val);

        int bar_w = (int)(norm * (float)thickness);
        if (bar_w <= 0)
        {
            continue;
        }

        // Тёмная подложка под баром (для читаемости на светлом фоне)
        overlay_rect_draw((rect_s) {0, sy, bar_w + outline, sy + 1}, (fcolor_rgba_s) {0.0f, 0.0f, 0.0f, 0.45f});
        // Сам бар
        overlay_rect_draw((rect_s) {0, sy, bar_w, sy + 1}, UI_LINE_PROFILE_Y);
    }

    //-------------------------------------------------------
    // Нижняя полоса: горизонтальный профиль (срез по строке Y=cursor)
    //-------------------------------------------------------
    for (int sx = 0; sx < win.x; sx++)
    {
        point_s f;
        if (!viewport_screen_to_frame((point_s) {sx, mouse.y}, &f))
        {
            continue;
        }
        if (f.x < 0 || f.x >= fsize.x)
        {
            continue;
        }

        frame_pixel_t px = frame_context_read(f.x, cursor_frame.y);
        float norm       = pixel_norm(px, bpp, max_val);

        int bar_h = (int)(norm * (float)thickness);
        if (bar_h <= 0)
        {
            continue;
        }

        int y_top = win.y - bar_h - outline;

        // Тёмная подложка
        overlay_rect_draw((rect_s) {sx, y_top, sx + 1, win.y}, (fcolor_rgba_s) {0.0f, 0.0f, 0.0f, 0.45f});
        // Сам бар
        overlay_rect_draw((rect_s) {sx, win.y - bar_h, sx + 1, win.y}, UI_LINE_PROFILE_X);
    }
}
