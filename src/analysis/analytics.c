#include "analytics.h"
#include "render/gl_api.h"


#include <string.h>

static histogram_s g_histogram;
static bool g_is_ycbcr = false;

/// \brief Проверяет, является ли текущий формат YCbCr.
static bool format_is_ycbcr(const char * format_name)
{
    return format_name && strcmp(format_name, "ycbcr") == 0;
}

/// \brief Считает гистограмму по прямоугольной области кадра.
static void histogram_rect(const uint8_t * data, int img_w, const format_config_s * fmt, int rx, int ry, int rw, int rh)
{
    histogram_reset(&g_histogram);

    if (rw <= 0 || rh <= 0)
    {
        g_histogram.active_channels = 0;
        return;
    }

    const int step = (fmt->gl_format == GL_RGBA) ? 4 : 3;

    if (fmt->gl_format == GL_RED && fmt->gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 1;
        for (int y = ry; y < ry + rh; y++)
        {
            const uint8_t * row = data + ((size_t)y * (size_t)img_w);
            for (int x = rx; x < rx + rw; x++)
            {
                histogram_add(&g_histogram, 0, row[ x ]);
            }
        }
    }
    else if (fmt->gl_format == GL_RED && fmt->gl_type == GL_UNSIGNED_SHORT)
    {
        g_histogram.active_channels = 1;
        for (int y = ry; y < ry + rh; y++)
        {
            const uint16_t * row = (const uint16_t *)(data + ((size_t)y * (size_t)img_w * 2));
            for (int x = rx; x < rx + rw; x++)
            {
                histogram_add(&g_histogram, 0, row[ x ] >> 8);
            }
        }
    }
    else if ((fmt->gl_format == GL_RGB || fmt->gl_format == GL_RGBA) && fmt->gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 3;
        for (int y = ry; y < ry + rh; y++)
        {
            const uint8_t * row = data + ((size_t)y * (size_t)img_w * (size_t)step);
            for (int x = rx; x < rx + rw; x++)
            {
                const uint8_t * px = row + ((size_t)x * (size_t)step);
                histogram_add(&g_histogram, 0, px[ 0 ]);
                histogram_add(&g_histogram, 1, px[ 1 ]);
                histogram_add(&g_histogram, 2, px[ 2 ]);
            }
        }
    }
    else
    {
        g_histogram.active_channels = 0;
    }

    histogram_finalize(&g_histogram);
}

void analytics_update(const uint8_t * data, int width, int height, const format_config_s * fmt,
                      const char * format_name, const selection_s * sel)
{
    g_is_ycbcr = format_is_ycbcr(format_name);

    int rx = 0, ry = 0, rw = width, rh = height;

    if (sel && selection_is_meaningful(sel))
    {
        int x0, y0, x1, y1;
        selection_rect(sel, &x0, &y0, &x1, &y1);

        if (x0 < 0)
        {
            x0 = 0;
        }
        if (y0 < 0)
        {
            y0 = 0;
        }
        if (x1 > width)
        {
            x1 = width;
        }
        if (y1 > height)
        {
            y1 = height;
        }

        rx = x0;
        ry = y0;
        rw = x1 - x0;
        rh = y1 - y0;
    }

    histogram_rect(data, width, fmt, rx, ry, rw, rh);
}

const histogram_s * analytics_get_histogram(void)
{
    return &g_histogram;
}

bool analytics_format_is_ycbcr(void)
{
    return g_is_ycbcr;
}
