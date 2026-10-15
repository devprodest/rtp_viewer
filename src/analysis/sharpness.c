#include "sharpness.h"

#include <math.h>
#include <string.h>

/// \brief Возвращает яркость пикселя (0..255).
static int pixel_luma(const uint8_t * data, int width, const format_config_s * fmt, int x, int y)
{
    if (fmt->bytes_per_pixel == 1)
    {
        return data[(size_t)y * (size_t)width + (size_t)x];
    }
    if (fmt->bytes_per_pixel == 2)
    {
        const uint16_t * p = (const uint16_t *)data;
        return p[(size_t)y * (size_t)width + (size_t)x] >> 8;
    }
    if (fmt->bytes_per_pixel == 3)
    {
        size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 3;
        int    r   = data[idx + 0];
        int    g   = data[idx + 1];
        int    b   = data[idx + 2];
        return (r * 77 + g * 150 + b * 29) >> 8;
    }
    if (fmt->bytes_per_pixel == 4)
    {
        size_t idx = ((size_t)y * (size_t)width + (size_t)x) * 4;
        int    r   = data[idx + 0];
        int    g   = data[idx + 1];
        int    b   = data[idx + 2];
        return (r * 77 + g * 150 + b * 29) >> 8;
    }
    return 0;
}

/// \brief Определяет границы области анализа по выделению или по всему кадру.
static void get_roi(const selection_s * sel, int width, int height, int * x0, int * y0, int * x1, int * y1)
{
    *x0 = 0;
    *y0 = 0;
    *x1 = width;
    *y1 = height;

    if (sel && selection_is_meaningful(sel))
    {
        int rx0 = 0;
        int ry0 = 0;
        int rx1 = 0;
        int ry1 = 0;
        selection_rect(sel, &rx0, &ry0, &rx1, &ry1);

        if (rx0 < 0)      rx0 = 0;
        if (ry0 < 0)      ry0 = 0;
        if (rx1 > width)  rx1 = width;
        if (ry1 > height) ry1 = height;

        *x0 = rx0;
        *y0 = ry0;
        *x1 = rx1;
        *y1 = ry1;
    }
}

void sharpness_compute(const uint8_t * data, int width, int height, const format_config_s * fmt, const selection_s * sel,
                       sharpness_s * out)
{
    out->lap_var   = 0.0;
    out->tenengrad = 0.0;
    out->valid     = 0;

    if (!data || width < 3 || height < 3)
    {
        return;
    }

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    get_roi(sel, width, height, &x0, &y0, &x1, &y1);

    // Внутренние пиксели: от x0+1 до x1-2 включительно
    int ix0 = x0 + 1;
    int iy0 = y0 + 1;
    int ix1 = x1 - 1;
    int iy1 = y1 - 1;

    if (ix0 >= ix1 || iy0 >= iy1)
    {
        return;
    }

    // Laplacian: variance
    double lap_sum  = 0.0;
    double lap_sum2 = 0.0;
    long   lap_n    = 0;

    // Tenengrad: сумма квадратов Sobel
    double ten_sum = 0.0;

    for (int y = iy0; y < iy1; y++)
    {
        for (int x = ix0; x < ix1; x++)
        {
            int c = pixel_luma(data, width, fmt, x, y);
            int n = pixel_luma(data, width, fmt, x, y - 1);
            int s = pixel_luma(data, width, fmt, x, y + 1);
            int w = pixel_luma(data, width, fmt, x - 1, y);
            int e = pixel_luma(data, width, fmt, x + 1, y);

            // Laplacian 4-связный
            int lap = n + s + w + e - 4 * c;

            lap_sum  += (double)lap;
            lap_sum2 += (double)lap * (double)lap;
            lap_n++;

            // Sobel
            int nw = pixel_luma(data, width, fmt, x - 1, y - 1);
            int ne = pixel_luma(data, width, fmt, x + 1, y - 1);
            int sw = pixel_luma(data, width, fmt, x - 1, y + 1);
            int se = pixel_luma(data, width, fmt, x + 1, y + 1);

            int gx = (ne + 2 * e + se) - (nw + 2 * w + sw);
            int gy = (sw + 2 * s + se) - (nw + 2 * n + ne);

            ten_sum += (double)(gx * gx + gy * gy);
        }
    }

    if (lap_n == 0)
    {
        return;
    }

    double lap_mean = lap_sum / (double)lap_n;
    double lap_var  = (lap_sum2 / (double)lap_n) - (lap_mean * lap_mean);
    if (lap_var < 0.0)
    {
        lap_var = 0.0;
    }

    out->lap_var   = lap_var;
    out->tenengrad = ten_sum / (double)lap_n;
    out->valid     = 1;
}