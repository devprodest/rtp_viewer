#include "diff_view.h"

#include "log/logger.h"
#include "render/ui/pixel_probe.h"
#include "util/utils.h"

#include <stdlib.h>
#include <string.h>

static uint8_t * g_reference_buffer = nullptr;
static size_t g_reference_bytes     = 0;

static uint8_t * g_diff_buffer = nullptr;
static size_t g_diff_bytes     = 0;

static bool g_diff_enabled = false;
static float g_diff_gain   = 4.0f;

void diff_view_cleanup(void)
{
    free(g_reference_buffer);
    g_reference_buffer = nullptr;
    g_reference_bytes  = 0;

    free(g_diff_buffer);
    g_diff_buffer = nullptr;
    g_diff_bytes  = 0;

    g_diff_enabled = false;
}

void diff_view_set_reference(void)
{
    const uint8_t * data = pixel_probe_get_frame();
    if (!data)
    {
        log_info("Reference: no frame available yet");
        return;
    }

    image_size_s size = pixel_probe_get_frame_size();
    int bpp           = pixel_probe_get_bpp();
    size_t bytes      = (size_t)size.x * (size_t)size.y * (size_t)bpp;

    if (bytes == 0)
    {
        return;
    }

    if (g_reference_bytes != bytes)
    {
        free(g_reference_buffer);
        g_reference_buffer = (uint8_t *)malloc(bytes);
        if (!g_reference_buffer)
        {
            g_reference_bytes = 0;
            log_error("diff_view: out of memory (%zu bytes)", bytes);
            return;
        }
        g_reference_bytes = bytes;
    }

    memcpy(g_reference_buffer, data, bytes);
    log_info("Reference frame set (%dx%d, %zu bytes)", size.x, size.y, bytes);
}

void diff_view_toggle(void)
{
    if (!g_reference_buffer)
    {
        log_info("Diff: no reference set (press V first)");
        return;
    }
    g_diff_enabled = !g_diff_enabled;
    log_info("Diff %s (gain %.0fx)", g_diff_enabled ? "enabled" : "disabled", (double)g_diff_gain);
}

bool diff_view_is_enabled(void)
{
    return g_diff_enabled;
}

void diff_view_gain_up(void)
{
    g_diff_gain = utils_min(g_diff_gain * 2.0f, 64.0f);
    log_info("Diff gain = %.0fx", (double)g_diff_gain);
}

void diff_view_gain_down(void)
{
    g_diff_gain = utils_max(g_diff_gain / 2.0f, 1.0f);
    log_info("Diff gain = %.0fx", (double)g_diff_gain);
}

static void compute_diff(const uint8_t * cur, const uint8_t * ref, uint8_t * out, size_t bytes, int bpp,
                         int effective_bits)
{
    const float gain = g_diff_gain;

    if (bpp == 2)
    {
        const uint16_t * c = (const uint16_t *)cur;
        const uint16_t * r = (const uint16_t *)ref;
        uint16_t * o       = (uint16_t *)out;
        const size_t n     = bytes / 2;
        const int maxval   = (effective_bits >= 16) ? 65535 : ((1 << effective_bits) - 1);

        for (size_t i = 0; i < n; i++)
        {
            int d  = (int)c[ i ] - (int)r[ i ];
            d      = utils_abs(d);
            int v  = (int)((float)d * gain + 0.5f);
            o[ i ] = (uint16_t)utils_min(v, maxval);
        }
    }
    else
    {
        for (size_t i = 0; i < bytes; i++)
        {
            int d    = (int)cur[ i ] - (int)ref[ i ];
            d        = utils_abs(d);
            int v    = (int)((float)d * gain + 0.5f);
            out[ i ] = (uint8_t)utils_min(v, 255);
        }
    }
}

const uint8_t * diff_view_process(const uint8_t * data, image_size_s size, int bpp, int effective_bits)
{
    if (!g_diff_enabled || !g_reference_buffer || !data)
    {
        return data;
    }

    size_t bytes = (size_t)size.x * (size_t)size.y * (size_t)bpp;

    if (bytes == 0 || bytes != g_reference_bytes)
    {
        return data;
    }

    if (g_diff_bytes != bytes)
    {
        free(g_diff_buffer);
        g_diff_buffer = (uint8_t *)malloc(bytes);
        g_diff_bytes  = g_diff_buffer ? bytes : 0;
    }

    if (g_diff_buffer)
    {
        compute_diff(data, g_reference_buffer, g_diff_buffer, bytes, bpp, effective_bits);
        return g_diff_buffer;
    }

    return data;
}
