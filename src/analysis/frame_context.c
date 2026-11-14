#include "frame_context.h"
#include "util/color.h"
#include "util/utils.h"

static const uint8_t * g_data = nullptr;
static image_size_s g_size    = {0, 0};
static format_config_s g_fmt;
static int g_bits   = 8;
static bool g_valid = false;

void frame_context_set(const uint8_t * data, image_size_s size, const format_config_s * fmt, int effective_bits)
{
    g_data  = data;
    g_size  = size;
    g_fmt   = *fmt;
    g_bits  = effective_bits;
    g_valid = (data != nullptr);
}

void frame_context_clear(void)
{
    g_data  = nullptr;
    g_valid = false;
}

const uint8_t * frame_context_data(void)
{
    return g_data;
}
image_size_s frame_context_size(void)
{
    return g_size;
}
int frame_context_bpp(void)
{
    return g_fmt.bytes_per_pixel;
}
int frame_context_bits(void)
{
    return g_bits;
}
const format_config_s * frame_context_format(void)
{
    return &g_fmt;
}
bool frame_context_is_valid(void)
{
    return g_valid;
}

frame_pixel_t frame_context_read(int x, int y)
{
    frame_pixel_t px = {0};

    if (!g_valid || x < 0 || y < 0 || x >= g_size.x || y >= g_size.y)
    {
        return px;
    }

    size_t idx = (size_t)y * (size_t)g_size.x + (size_t)x;

    if (g_fmt.bytes_per_pixel == 1)
    {
        uint8_t v = g_data[ idx ];
        px.raw    = v;
        px.r = px.g = px.b = v;
        px.luma            = v;
    }
    else if (g_fmt.bytes_per_pixel == 2)
    {
        const uint16_t * p = (const uint16_t *)g_data;
        uint16_t v         = p[ idx ];
        px.raw             = v;

        const int shift  = utils_clamp(g_bits - 8, 0, 8);
        const uint8_t v8 = (uint8_t)(v >> shift);
        px.r = px.g = px.b = v8;
        px.luma            = v8;
    }
    else if (g_fmt.bytes_per_pixel >= 3)
    {
        size_t j = idx * (size_t)g_fmt.bytes_per_pixel;
        px.r     = g_data[ j + 0 ];
        px.g     = g_data[ j + 1 ];
        px.b     = g_data[ j + 2 ];
        // raw не заполняем для цветных (используйте luma или r/g/b).

        if (g_fmt.is_ycbcr)
        {
            px.luma = px.r; // Y-канал
        }
        else
        {
            px.luma = color_rgb_to_luma601(px.r, px.g, px.b);
        }
    }

    return px;
}
