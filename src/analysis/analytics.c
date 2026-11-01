#include "analytics.h"
#include "render/gl_api.h"
#include "util/utils.h"

#include <float.h>
#include <math.h>
#include <string.h>

static histogram_s g_histogram;
static bool g_is_ycbcr = false;

static analytics_stats_s g_stats;
static double g_sum[ 3 ];
static double g_sum_sq[ 3 ];
static bool g_stats_visible = false;

const analytics_stats_s * analytics_get_stats(void)
{
    return &g_stats;
}
void analytics_toggle_stats_visible(void)
{
    g_stats_visible = !g_stats_visible;
}
bool analytics_is_stats_visible(void)
{
    return g_stats_visible;
}

static inline void stats_reset(int channels)
{
    g_stats          = (analytics_stats_s) {0};
    g_stats.channels = channels;
    for (int c = 0; c < 3; c++)
    {
        g_stats.min[ c ] = DBL_MAX;
        g_stats.max[ c ] = -DBL_MAX;
        g_sum[ c ]           = 0.0;
        g_sum_sq[ c ]        = 0.0;
    }
}

static inline void stats_accum(int c, double v)
{
    if (v < g_stats.min[ c ])
    {
        g_stats.min[ c ] = v;
    }
    if (v > g_stats.max[ c ])
    {
        g_stats.max[ c ] = v;
    }
    g_sum[ c ] += v;
    g_sum_sq[ c ] += v * v;
}

static void stats_finalize(void)
{
    if (g_stats.count == 0)
    {
        g_stats.valid = false;
        return;
    }
    const double n = (double)g_stats.count;
    for (int c = 0; c < g_stats.channels; c++)
    {
        double mean = g_sum[ c ] / n;
        double var  = g_sum_sq[ c ] / n - mean * mean;
        if (var < 0.0)
        {
            var = 0.0;
        }
        g_stats.mean[ c ] = mean;
        g_stats.std[ c ]  = sqrt(var);
    }
    g_stats.valid = true;
}
/// \brief Считает гистограмму по прямоугольной области кадра.
static void histogram_rect(const uint8_t * data, int img_w, const format_config_s * fmt, point_s start,
                           image_size_s size, int effective_bits)
{
    histogram_reset(&g_histogram);

    if (size.x <= 0 || size.y <= 0)
    {
        g_histogram.active_channels = 0;
        stats_reset(0);
        return;
    }

    const int step      = (fmt->gl_format == GL_RGBA) ? 4 : 3;
    const uint64_t n_px = (uint64_t)size.x * (uint64_t)size.y;

    if (fmt->gl_format == GL_RED && fmt->gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 1;
        stats_reset(1);
        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint8_t * row = data + ((size_t)y * (size_t)img_w);
            for (int x = start.x; x < start.x + size.x; x++)
            {
                const uint8_t v = row[ x ];
                histogram_add(&g_histogram, 0, v);
                stats_accum(0, (double)v);
            }
        }
        g_stats.count = n_px;
    }
    else if (fmt->gl_format == GL_RED && fmt->gl_type == GL_UNSIGNED_SHORT)
    {
        g_histogram.active_channels = 1;
        stats_reset(1);

        // Для bits >= 8: v >> (bits - 8). Для bits < 8: shift = 0, значения
        // остаются как есть (гистограмма сжата в младшие 2^bits бинов).
        int shift = utils_clamp(effective_bits - 8, 0, 8);

        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint16_t * row = (const uint16_t *)(data + ((size_t)y * (size_t)img_w * 2));
            for (int x = start.x; x < start.x + size.x; x++)
            {
                const uint16_t v = row[ x ];
                histogram_add(&g_histogram, 0, v >> shift);
                stats_accum(0, (double)v); // raw, без shift
            }
        }
        g_stats.count = n_px;
    }
    else if ((fmt->gl_format == GL_RGB || fmt->gl_format == GL_RGBA) && fmt->gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 3;
        stats_reset(3);
        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint8_t * row = data + ((size_t)y * (size_t)img_w * (size_t)step);
            for (int x = start.x; x < start.x + size.x; x++)
            {
                const uint8_t * px = row + ((size_t)x * (size_t)step);
                histogram_add(&g_histogram, 0, px[ 0 ]);
                histogram_add(&g_histogram, 1, px[ 1 ]);
                histogram_add(&g_histogram, 2, px[ 2 ]);
                stats_accum(0, (double)px[ 0 ]);
                stats_accum(1, (double)px[ 1 ]);
                stats_accum(2, (double)px[ 2 ]);
            }
        }
        g_stats.count = n_px;
    }
    else
    {
        g_histogram.active_channels = 0;
        stats_reset(0); // count = 0 → finalize поставит valid = false
    }

    histogram_finalize(&g_histogram);
    stats_finalize();
}

void analytics_update(const uint8_t * data, image_size_s win, const format_config_s * fmt, const selection_s * sel,
                      int effective_bits)
{
    g_is_ycbcr = fmt->is_ycbcr;

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

    histogram_rect(data, win.x, fmt, start, size, effective_bits);
}

const histogram_s * analytics_get_histogram(void)
{
    return &g_histogram;
}

bool analytics_format_is_ycbcr(void)
{
    return g_is_ycbcr;
}
