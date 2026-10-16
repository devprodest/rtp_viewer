#include "sharpness.h"

#include <math.h>
#include <string.h>

/// \brief Возвращает яркость пикселя (0..255).
static int pixel_luma(const uint8_t * data, int width, const format_config_s * fmt, int x, int y)
{
    if (fmt->bytes_per_pixel == 1)
    {
        return data[ (size_t)y * (size_t)width + (size_t)x ];
    }
    if (fmt->bytes_per_pixel == 2)
    {
        const uint16_t * p = (const uint16_t *)data;
        return p[ (size_t)y * (size_t)width + (size_t)x ] >> 8;
    }
    if (fmt->bytes_per_pixel == 3)
    {
        size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 3;
        int r      = data[ idx + 0 ];
        int g      = data[ idx + 1 ];
        int b      = data[ idx + 2 ];
        return (r * 77 + g * 150 + b * 29) >> 8;
    }
    if (fmt->bytes_per_pixel == 4)
    {
        size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 4;
        int r      = data[ idx + 0 ];
        int g      = data[ idx + 1 ];
        int b      = data[ idx + 2 ];
        return (r * 77 + g * 150 + b * 29) >> 8;
    }
    return 0;
}

/// \brief Определяет границы области анализа по выделению или по всему кадру.
static rect_s get_roi(const selection_s * sel, image_size_s win)
{
    if (sel && selection_is_meaningful(sel))
    {
        rect_s select = selection_rect(sel);

        if (select.x0 < 0)
        {
            select.x0 = 0;
        }
        if (select.y0 < 0)
        {
            select.y0 = 0;
        }
        if (select.x1 > win.x)
        {
            select.x1 = win.x;
        }
        if (select.y1 > win.y)
        {
            select.y1 = win.y;
        }

        return select;
    }

    return (rect_s) {0, 0, win.x, win.y};
}

sharpness_s sharpness_compute(const uint8_t * data, image_size_s win, const format_config_s * fmt,
                              const selection_s * sel)
{
    sharpness_s out = {0};

    if (!data || win.x < 3 || win.y < 3)
    {
        return out;
    }

    rect_s roi = get_roi(sel, win);

    // Внутренние пиксели: от x0+1 до x1-2 включительно
    roi.x0 += 1;
    roi.y0 += 1;
    roi.x1 -= 1;
    roi.y1 -= 1;

    if (roi.x0 >= roi.x1 || roi.y0 >= roi.y1)
    {
        return out;
    }

    // Laplacian: variance
    double lap_sum  = 0.0;
    double lap_sum2 = 0.0;
    long lap_n      = 0;

    // Tenengrad: сумма квадратов Sobel
    double ten_sum = 0.0;

    for (int y = roi.y0; y < roi.y1; y++)
    {
        for (int x = roi.x0; x < roi.x1; x++)
        {
            int c = pixel_luma(data, win.x, fmt, x, y);
            int n = pixel_luma(data, win.x, fmt, x, y - 1);
            int s = pixel_luma(data, win.x, fmt, x, y + 1);
            int w = pixel_luma(data, win.x, fmt, x - 1, y);
            int e = pixel_luma(data, win.x, fmt, x + 1, y);

            // Laplacian 4-связный
            int lap = n + s + w + e - 4 * c;

            lap_sum += (double)lap;
            lap_sum2 += (double)lap * (double)lap;
            lap_n++;

            // Sobel
            int nw = pixel_luma(data, win.x, fmt, x - 1, y - 1);
            int ne = pixel_luma(data, win.x, fmt, x + 1, y - 1);
            int sw = pixel_luma(data, win.x, fmt, x - 1, y + 1);
            int se = pixel_luma(data, win.x, fmt, x + 1, y + 1);

            int gx = (ne + 2 * e + se) - (nw + 2 * w + sw);
            int gy = (sw + 2 * s + se) - (nw + 2 * n + ne);

            ten_sum += (double)(gx * gx + gy * gy);
        }
    }

    if (lap_n == 0)
    {
        return out;
    }

    double lap_mean = lap_sum / (double)lap_n;
    double lap_var  = (lap_sum2 / (double)lap_n) - (lap_mean * lap_mean);
    if (lap_var < 0.0)
    {
        lap_var = 0.0;
    }

    out.lap_var   = lap_var;
    out.tenengrad = ten_sum / (double)lap_n;
    out.valid     = 1;

    return out;
}
