#include "analytics.h"

#include "log/logger.h"
#include "util/utils.h"

#include <math.h>
#include <string.h>

static histogram_s g_histogram;
static analytics_stats_s g_stats;
static analytics_vectorscope_s g_vectorscope;
static bool g_is_ycbcr      = false;
static bool g_stats_visible = false;

//------------------------------------------------------- Утилиты ------------------------------------------------------

static void stats_reset(int channels)
{
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.channels = channels;

    for (int c = 0; c < 3; c++)
    {
        g_stats.min[ c ] = 1e30;
        g_stats.max[ c ] = -1e30;
    }
}

static void stats_add(int c, double v, double * sum, double * sum_sq)
{
    if (v < g_stats.min[ c ])
    {
        g_stats.min[ c ] = v;
    }
    if (v > g_stats.max[ c ])
    {
        g_stats.max[ c ] = v;
    }
    sum[ c ] += v;
    sum_sq[ c ] += v * v;
}

static void stats_finalize(const double * sum, const double * sum_sq)
{
    if (g_stats.count == 0)
    {
        g_stats.valid = false;
        return;
    }
    const double n = (double)g_stats.count;
    for (int c = 0; c < g_stats.channels; c++)
    {
        double mean = sum[ c ] / n;
        double var  = sum_sq[ c ] / n - mean * mean;
        if (var < 0.0)
        {
            var = 0.0;
        }
        g_stats.mean[ c ] = mean;
        g_stats.std[ c ]  = sqrt(var);
    }
    g_stats.valid = true;
}

//------------------------------------------------------- Проход
//--------------------------------------------------------

void analytics_update(const uint8_t * data, image_size_s win, const format_config_s * fmt, const selection_s * sel,
                      int effective_bits)
{
    histogram_reset(&g_histogram);
    g_is_ycbcr    = fmt->is_ycbcr;
    g_stats.valid = false;
    memset(&g_vectorscope, 0, sizeof(g_vectorscope));
    g_vectorscope.valid = false;

    if (!data)
    {
        return;
    }

    point_s start     = {0};
    image_size_s size = win;

    if (sel && selection_is_meaningful(sel))
    {
        rect_s r = selection_rect(sel);
        r.x0     = utils_max(r.x0, 0);
        r.y0     = utils_max(r.y0, 0);
        r.x1     = utils_min(r.x1, win.x);
        r.y1     = utils_min(r.y1, win.y);

        start.x = r.x0;
        start.y = r.y0;
        size.x  = r.x1 - r.x0;
        size.y  = r.y1 - r.y0;
    }

    if (size.x <= 0 || size.y <= 0)
    {
        g_histogram.active_channels = 0;
        stats_reset(0);
        return;
    }

    double sum[ 3 ]     = {0.0, 0.0, 0.0};
    double sum_sq[ 3 ]  = {0.0, 0.0, 0.0};
    const uint64_t n_px = (uint64_t)size.x * (uint64_t)size.y;

    const int bpp  = fmt->bytes_per_pixel;
    const int step = (bpp == 4) ? 4 : 3;

    if (bpp == 1)
    {
        g_histogram.active_channels = 1;
        stats_reset(1);

        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint8_t * row = data + (size_t)y * (size_t)win.x;
            for (int x = start.x; x < start.x + size.x; x++)
            {
                const uint8_t v = row[ x ];
                histogram_add(&g_histogram, 0, v);
                stats_add(0, (double)v, sum, sum_sq);
            }
        }
        g_stats.count = n_px;
    }
    else if (bpp == 2)
    {
        g_histogram.active_channels = 1;
        stats_reset(1);

        const int shift = utils_clamp(effective_bits - 8, 0, 8);

        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint16_t * row = (const uint16_t *)(data + (size_t)y * (size_t)win.x * 2);
            for (int x = start.x; x < start.x + size.x; x++)
            {
                const uint16_t v = row[ x ];
                histogram_add(&g_histogram, 0, v >> shift);
                stats_add(0, (double)v, sum, sum_sq);
            }
        }
        g_stats.count = n_px;
    }
    else if (bpp >= 3)
    {
        g_histogram.active_channels = 3;
        stats_reset(3);

        const bool is_ycbcr = fmt->is_ycbcr;

        for (int y = start.y; y < start.y + size.y; y++)
        {
            const uint8_t * row = data + (size_t)y * (size_t)win.x * (size_t)step;
            for (int x = start.x; x < start.x + size.x; x++)
            {
                const uint8_t * px = row + (size_t)x * (size_t)step;

                histogram_add(&g_histogram, 0, px[ 0 ]);
                histogram_add(&g_histogram, 1, px[ 1 ]);
                histogram_add(&g_histogram, 2, px[ 2 ]);
                stats_add(0, (double)px[ 0 ], sum, sum_sq);
                stats_add(1, (double)px[ 1 ], sum, sum_sq);
                stats_add(2, (double)px[ 2 ], sum, sum_sq);

                // Cb/Cr для vectorscope
                int cb, cr;
                if (is_ycbcr)
                {
                    cb = px[ 1 ];
                    cr = px[ 2 ];
                }
                else
                {
                    cb = (int)(-0.169f * px[ 0 ] - 0.331f * px[ 1 ] + 0.500f * px[ 2 ] + 128.0f);
                    cr = (int)(0.500f * px[ 0 ] - 0.419f * px[ 1 ] - 0.081f * px[ 2 ] + 128.0f);
                }

                cb = utils_clamp(cb, 0, ANALYTICS_VS_BINS - 1);
                cr = utils_clamp(cr, 0, ANALYTICS_VS_BINS - 1);

                uint32_t v = ++g_vectorscope.bins[ cr ][ cb ];
                if (v > g_vectorscope.max_count)
                {
                    g_vectorscope.max_count = v;
                }
            }
        }
        g_stats.count = n_px;

        g_vectorscope.valid = (g_vectorscope.max_count > 0);
    }
    else
    {
        g_histogram.active_channels = 0;
        stats_reset(0);
        return;
    }

    histogram_finalize(&g_histogram);
    stats_finalize(sum, sum_sq);
}

//------------------------------------------------------- Геттеры ------------------------------------------------------

const analytics_stats_s * analytics_get_stats(void)
{
    return &g_stats;
}

const histogram_s * analytics_get_histogram(void)
{
    return &g_histogram;
}

const analytics_vectorscope_s * analytics_get_vectorscope(void)
{
    return &g_vectorscope;
}

bool analytics_format_is_ycbcr(void)
{
    return g_is_ycbcr;
}

//------------------------------------------------------- Оверлей stats ------------------------------------------------

void analytics_toggle_stats_visible(void)
{
    g_stats_visible = !g_stats_visible;
    log_info("Stats %s", g_stats_visible ? "enabled" : "disabled");
}

bool analytics_is_stats_visible(void)
{
    return g_stats_visible;
}

void analytics_disable_stats(void)
{
    if (!g_stats_visible)
    {
        return;
    }
    g_stats_visible = false;
    log_info("Stats disabled");
}
