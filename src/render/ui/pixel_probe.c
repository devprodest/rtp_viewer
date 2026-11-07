#include "pixel_probe.h"

#include "render/overlay.h"
#include "render/ui/ui_color_scheme.h"
#include "render/viewport.h"
#include "util/utils.h"

#include <stdio.h>

//------------------------------------------------------- Состояние ---------------------------------------------------

static bool g_enabled               = false;
static const uint8_t * g_frame_data = nullptr;
static image_size_s g_frame_size    = {0, 0};
static int g_bytes_per_pixel        = 0;
static point_s g_mouse              = {-1, -1};

//------------------------------------------------------- API ---------------------------------------------------------

void pixel_probe_toggle(void)
{
    g_enabled = !g_enabled;
}

bool pixel_probe_is_enabled(void)
{
    return g_enabled;
}

void pixel_probe_set_frame(const uint8_t * data, image_size_s frame_size, int bbp)
{
    g_frame_data      = data;
    g_frame_size      = frame_size;
    g_bytes_per_pixel = bbp;
}

void pixel_probe_set_mouse(point_s screen)
{
    g_mouse = screen;
}

//------------------------------------------------------- Помощники ---------------------------------------------------

/// \brief Читает пиксель из текущего кадра. Возвращает RGB и raw-значение.
pixel_sample_s pixel_probe_read_pixel(point_s pixel)
{
    pixel_sample_s sample = {0};

    if (!g_frame_data)
    {
        return sample;
    }
    if (pixel.x < 0 || pixel.y < 0 || pixel.x >= g_frame_size.x || pixel.y >= g_frame_size.y)
    {
        return sample;
    }

    size_t idx = (size_t)pixel.y * (size_t)g_frame_size.x + (size_t)pixel.x;

    switch (g_bytes_per_pixel)
    {
        case 1:
        {
            uint8_t v  = g_frame_data[ idx ];
            sample.rgb = (color_rgb_s) {v, v, v};
            sample.raw = v;
            break;
        }
        case 2:
        {
            const uint16_t * p = (const uint16_t *)g_frame_data;
            uint16_t v         = p[ idx ];
            uint8_t v8         = (uint8_t)(v >> 8);
            sample.rgb         = (color_rgb_s) {v8, v8, v8};
            sample.raw         = v;
            break;
        }
        case 3:
        {
            size_t j     = idx * 3;
            sample.rgb.r = g_frame_data[ j + 0 ];
            sample.rgb.g = g_frame_data[ j + 1 ];
            sample.rgb.b = g_frame_data[ j + 2 ];
            break;
        }
        case 4:
        {
            size_t j     = idx * 4;
            sample.rgb.r = g_frame_data[ j + 0 ];
            sample.rgb.g = g_frame_data[ j + 1 ];
            sample.rgb.b = g_frame_data[ j + 2 ];
            break;
        }
        default: break;
    }
    return sample;
}

point_s pixel_probe_get_mouse(void)
{
    return g_mouse;
}

image_size_s pixel_probe_get_frame_size(void)
{
    return g_frame_size;
}

int pixel_probe_get_bpp(void)
{
    return g_bytes_per_pixel;
}

//------------------------------------------------------- Отрисовка ---------------------------------------------------

void pixel_probe_draw(image_size_s win, float dpi_scale)
{
    if (!g_enabled || !g_frame_data)
    {
        return;
    }
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }

    const int mx = g_mouse.x;
    const int my = g_mouse.y;
    if (mx < 0 || my < 0 || mx >= win.x || my >= win.y)
    {
        return;
    }

    // Переводим курсор в координаты кадра
    point_s frame;
    if (!viewport_screen_to_frame(g_mouse, &frame))
    {
        return;
    }
    if (frame.x < 0 || frame.x >= g_frame_size.x)
    {
        return;
    }
    if (frame.y < 0 || frame.y >= g_frame_size.y)
    {
        return;
    }

    // ----- Перекрестие на весь экран -----
    int lw = utils_max(1, (int)(1 * dpi_scale));
    overlay_rect_draw((rect_s) {0, my, win.x, my + lw}, UI_CROSSHAIR_LINE);
    overlay_rect_draw((rect_s) {mx, 0, mx + lw, win.y}, UI_CROSSHAIR_LINE);

    // ----- Читаем пиксель -----
    pixel_sample_s sample = pixel_probe_read_pixel(frame);

    // ----- Формируем строки -----
    char line1[ 64 ];
    char line2[ 64 ];
    snprintf(line1, sizeof(line1), "x;y=(%d;%d)", frame.x, frame.y);
    if (g_bytes_per_pixel <= 2)
    {
        snprintf(line2, sizeof(line2), "val=%u", sample.raw);
    }
    else
    {
        snprintf(line2, sizeof(line2), "RGB=(%u;%u;%u)", sample.rgb.r, sample.rgb.g, sample.rgb.b);
    }

    // ----- Геометрия панели -----
    const float text_scale = dpi_scale * 1.2f;
    const int pad          = 6 * dpi_scale;
    const int box_size     = 40 * dpi_scale;
    const int gap          = 10 * dpi_scale;
    const int line_h       = 14 * text_scale;

    const int w1     = overlay_text_measure(line1, text_scale);
    const int w2     = overlay_text_measure(line2, text_scale);
    const int text_w = (w1 > w2) ? w1 : w2;

    // Высота контента: берём максимум из высоты текста и высоты квадрата
    const int text_h    = 2 * line_h;
    const int content_h = utils_max(box_size, text_h);

    const int panel_w = pad + box_size + gap + text_w + pad;
    const int panel_h = pad + content_h + pad;

    // Смещение от курсора; если не влезает — флипаем на другую сторону
    const int offset = (int)(14 * dpi_scale);

    int px = mx + offset;
    int py = my + offset;

    if (px + panel_w > win.x)
    {
        px = mx - panel_w - offset;
    }
    if (py + panel_h > win.y)
    {
        py = my - panel_h - offset;
    }

    // И только в конце подстраховываемся clamp'ом на случай слишком маленького окна
    px = utils_clamp(px, 0, utils_max(0, win.x - panel_w));
    py = utils_clamp(py, 0, utils_max(0, win.y - panel_h));

    rect_s panel = {px, py, px + panel_w, py + panel_h};
    overlay_rect_draw(panel, UI_PANEL_BG_STRONG);

    // ----- Квадрат цвета (центрируем вертикально) -----
    int box_y        = py + pad + (content_h - box_size) / 2;
    rect_s pix_color = {px + pad, box_y, px + pad + box_size, box_y + box_size};
    overlay_rect_draw(pix_color,
                      (fcolor_rgba_s) {sample.rgb.r / 255.0f, sample.rgb.g / 255.0f, sample.rgb.b / 255.0f, 1.0f});
    overlay_rect_outline_draw(pix_color, UI_PANEL_BORDER, 1);

    // ----- Текст (тоже центрируем) -----
    int text_y = py + pad + (content_h - text_h) / 2;
    int tx     = px + pad + box_size + gap;
    overlay_text_draw((point_s) {tx, text_y}, line1, UI_TEXT_PRIMARY, text_scale);
    overlay_text_draw((point_s) {tx, text_y + line_h}, line2, UI_TEXT_PRIMARY, text_scale);
}

const uint8_t * pixel_probe_get_frame(void)
{
    return g_frame_data;
}
