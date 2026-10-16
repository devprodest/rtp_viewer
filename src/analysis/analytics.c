#include "analytics.h"
#include "render/gl_api.h"
#include "util/utils.h"


#include <string.h>

static histogram_s g_histogram;
static bool g_is_ycbcr = false;

/// \brief Проверяет, является ли текущий формат YCbCr.
static bool format_is_ycbcr(const char * format_name)
{
    return format_name && strcmp(format_name, "ycbcr") == 0;
}

/// \brief Считает гистограмму по прямоугольной области кадра.
static void histogram_rect(const uint8_t * data, int img_w, const format_config_s * fmt, point_s start,
                           image_size_s size)
{
    histogram_reset(&g_histogram);

    if (size.x <= 0 || size.y <= 0)
    {
        g_histogram.active_channels = 0;
        return;
    }

    const int step = (fmt->gl_format == GL_RGBA) ? 4 : 3;

    if (fmt->gl_format == GL_RED && fmt->gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 1;
        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint8_t * row = data + ((size_t)y * (size_t)img_w);
            for (int x = start.x; x < start.x + size.x; x++)
            {
                histogram_add(&g_histogram, 0, row[ x ]);
            }
        }
    }
    else if (fmt->gl_format == GL_RED && fmt->gl_type == GL_UNSIGNED_SHORT)
    {
        g_histogram.active_channels = 1;
        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint16_t * row = (const uint16_t *)(data + ((size_t)y * (size_t)img_w * 2));
            for (int x = start.x; x < start.x + size.x; x++)
            {
                histogram_add(&g_histogram, 0, row[ x ] >> 8);
            }
        }
    }
    else if ((fmt->gl_format == GL_RGB || fmt->gl_format == GL_RGBA) && fmt->gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 3;
        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint8_t * row = data + ((size_t)y * (size_t)img_w * (size_t)step);
            for (int x = start.x; x < start.x + size.x; x++)
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

void analytics_update(const uint8_t * data, image_size_s win, const format_config_s * fmt, const char * format_name,
                      const selection_s * sel)
{
    g_is_ycbcr = format_is_ycbcr(format_name);

    point_s start     = {0};
    image_size_s size = win;

    if (sel && selection_is_meaningful(sel))
    {
        rect_s select = selection_rect(sel);

        select.x0 = utils_max(select.x0, 0);
        select.y0 = utils_max(select.y0, 0);
        select.x1 = utils_min(select.x1, win.x);
        select.y1 = utils_min(select.y1, win.y);

        start.x = select.x0;
        start.y = select.y0;
        size.x  = select.x1 - select.x0;
        size.y  = select.y1 - select.y0;
    }

    histogram_rect(data, win.x, fmt, start, size);
}

const histogram_s * analytics_get_histogram(void)
{
    return &g_histogram;
}

bool analytics_format_is_ycbcr(void)
{
    return g_is_ycbcr;
}
