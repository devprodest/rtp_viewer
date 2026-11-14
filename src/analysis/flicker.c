#include "flicker.h"

#include "analysis/frame_context.h"
#include "app/settings.h"
#include "log/logger.h"
#include "render/ui/ui_color_scheme.h"
#include "util/utils.h"

#include <math.h>
#include <stdlib.h>

#define FLICKER_DEFAULT_WINDOW 300
#define FLICKER_MAX_WINDOW     8192

static double * g_history = nullptr;
static int g_window       = 0;
static int g_head         = 0;
static int g_count        = 0;

//------------------------------------------------------- Внутреннее ---------------------------------------------------

static uint8_t chan_u8(float v)
{
    return (uint8_t)(utils_clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

static void pixel_set(uint8_t * p, fcolor_rgba_s c)
{
    p[ 0 ] = chan_u8(c.r);
    p[ 1 ] = chan_u8(c.g);
    p[ 2 ] = chan_u8(c.b);
    p[ 3 ] = chan_u8(c.a);
}

static void push_sample(double v)
{
    if (!g_history || g_window <= 0)
    {
        return;
    }
    g_history[ g_head ] = v;
    g_head              = (g_head + 1) % g_window;
    g_count             = utils_min(g_count + 1, g_window);
}

static double sample_at(int i)
{
    if (!g_history || g_window <= 0 || i < 0 || i >= g_count)
    {
        return 0.0;
    }
    const int oldest = ((g_head - g_count) % g_window + g_window) % g_window;
    return g_history[ (oldest + i) % g_window ];
}

/// \brief Границы анализа: выделение (если осмысленно) либо весь кадр.
static rect_s get_roi(const selection_s * sel, image_size_s win)
{
    rect_s roi = {0, 0, win.x, win.y};

    if (sel && selection_is_meaningful(sel))
    {
        roi    = selection_rect(sel);
        roi.x0 = utils_max(roi.x0, 0);
        roi.y0 = utils_max(roi.y0, 0);
        roi.x1 = utils_min(roi.x1, win.x);
        roi.y1 = utils_min(roi.y1, win.y);
    }
    return roi;
}

//------------------------------------------------------- API ---------------------------------------------------------

void flicker_init(void)
{
    flicker_reset();

    int w  = FLICKER_DEFAULT_WINDOW;
    int sw = 0;
    if (settings_get_feature_int("flicker", "window", &sw) && sw >= 10)
    {
        w = sw;
    }
    w = utils_min(w, FLICKER_MAX_WINDOW);

    g_window  = w;
    g_history = (double *)calloc((size_t)w, sizeof(double));
    if (!g_history)
    {
        g_window = 0;
        log_error("flicker: out of memory");
        return;
    }
    log_info("flicker: window = %d", g_window);
}

void flicker_reset(void)
{
    free(g_history);
    g_history = nullptr;
    g_window  = 0;
    g_head    = 0;
    g_count   = 0;
}

void flicker_update(const selection_s * sel)
{
    if (!frame_context_is_valid())
    {
        return;
    }

    const image_size_s win = frame_context_size();
    if (win.x <= 0 || win.y <= 0)
    {
        return;
    }

    const rect_s roi = get_roi(sel, win);
    if (roi.x0 >= roi.x1 || roi.y0 >= roi.y1)
    {
        return;
    }

    uint64_t sum   = 0;
    uint64_t count = 0;

    for (int y = roi.y0; y < roi.y1; y++)
    {
        for (int x = roi.x0; x < roi.x1; x++)
        {
            const frame_pixel_t px = frame_context_read(x, y);
            sum += px.luma;
            count++;
        }
    }

    if (count == 0)
    {
        return;
    }

    push_sample((double)sum / (double)count);
}

flicker_metrics_s flicker_get_metrics(void)
{
    flicker_metrics_s m = {0};
    if (!g_history || g_count == 0)
    {
        return m;
    }

    double mn     = 1e30;
    double mx     = -1e30;
    double sum    = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < g_count; i++)
    {
        const double v = sample_at(i);
        mn             = utils_min(mn, v);
        mx             = utils_max(mx, v);
        sum += v;
        sum_sq += v * v;
    }

    const double n = (double)g_count;
    m.cur          = sample_at(g_count - 1);
    m.min          = mn;
    m.max          = mx;
    m.mean         = sum / n;

    double var = sum_sq / n - m.mean * m.mean;
    m.std      = (var > 0.0) ? sqrt(var) : 0.0;

    m.peak_to_peak = mx - mn;
    m.count        = g_count;
    m.valid        = true;
    return m;
}

void flicker_render_rgba(uint8_t * rgba, int w, int h)
{
    if (!rgba || w <= 0 || h <= 0)
    {
        return;
    }

    const fcolor_rgba_s c_bg   = UI_FLICKER_BG;
    const fcolor_rgba_s c_line = UI_FLICKER_LINE;
    const fcolor_rgba_s c_mean = UI_FLICKER_MEAN;

    // Фон
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            pixel_set(rgba + ((size_t)y * (size_t)w + (size_t)x) * 4, c_bg);
        }
    }

    if (g_count < 2)
    {
        return;
    }

    const flicker_metrics_s m = flicker_get_metrics();
    if (!m.valid)
    {
        return;
    }

    double span        = m.max - m.min;
    span               = utils_max(span, 1.0);
    const double lo    = m.min - span * 0.05;
    const double hi    = m.max + span * 0.05;
    const double range = utils_max(hi - lo, 1e-6);

    int prev_y = -1;
    for (int i = 0; i < g_count; i++)
    {
        const int x    = (g_count > 1) ? (i * (w - 1)) / (g_count - 1) : 0;
        const double t = (sample_at(i) - lo) / range;
        int y_pix      = h - 1 - (int)(t * (h - 1) + 0.5);
        y_pix          = utils_clamp(y_pix, 0, h - 1);

        if (prev_y >= 0)
        {
            const int ya = utils_min(prev_y, y_pix);
            const int yb = utils_max(prev_y, y_pix);
            for (int yy = ya; yy <= yb; yy++)
            {
                pixel_set(rgba + ((size_t)yy * (size_t)w + (size_t)x) * 4, c_line);
            }
        }
        prev_y = y_pix;
    }

    // Средняя линия окна
    {
        const double t = (m.mean - lo) / range;
        int y_mean     = h - 1 - (int)(t * (h - 1) + 0.5);
        y_mean         = utils_clamp(y_mean, 0, h - 1);
        for (int x = 0; x < w; x++)
        {
            pixel_set(rgba + ((size_t)y_mean * (size_t)w + (size_t)x) * 4, c_mean);
        }
    }
}
