#include "color.h"

#include "util/utils.h"

u8color_rgb_s color_ycbcr_to_rgb(int y, int cb, int cr)
{
    const float r_f = (float)y + 1.402f * (float)(cr - 128);
    const float g_f = (float)y - 0.344136f * (float)(cb - 128) - 0.714136f * (float)(cr - 128);
    const float b_f = (float)y + 1.772f * (float)(cb - 128);

    return (u8color_rgb_s) {
        .r = (uint8_t)utils_clamp((int)(r_f + 0.5f), 0, 255),
        .g = (uint8_t)utils_clamp((int)(g_f + 0.5f), 0, 255),
        .b = (uint8_t)utils_clamp((int)(b_f + 0.5f), 0, 255),
    };
}

u8color_rgb_s color_rgb_to_ycbcr(int r, int g, int b)
{
    const float y_f  = 0.299f * (float)r + 0.587f * (float)g + 0.114f * (float)b;
    const float cb_f = -0.168736f * (float)r - 0.331264f * (float)g + 0.5f * (float)b + 128.0f;
    const float cr_f = 0.5f * (float)r - 0.418688f * (float)g - 0.081312f * (float)b + 128.0f;

    return (u8color_rgb_s) {
        .r = utils_clamp((int)(y_f + 0.5f), 0, 255),
        .g = utils_clamp((int)(cb_f + 0.5f), 0, 255),
        .b = utils_clamp((int)(cr_f + 0.5f), 0, 255),
    };
}

uint8_t color_rgb_to_luma601(int r, int g, int b)
{
    const float v = 0.299f * (float)r + 0.587f * (float)g + 0.114f * (float)b;
    return (uint8_t)utils_clamp((int)(v + 0.5f), 0, 255);
}
