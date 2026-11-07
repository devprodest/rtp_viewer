#include "vectorscope_view.h"

#include "log/logger.h"
#include "render/gl/gl_utils.h"
#include "ui_color_scheme.h"
#include "util/types.h"
#include "util/utils.h"

#include <math.h>
#include <string.h>

#define VS_BINS 256
#define VS_VIEW 240 // размер отображаемой панели

static GLuint g_quad_vao   = 0;
static GLuint g_tex        = 0;
static GLuint g_program    = 0;
static GLint g_uniform_tex = -1;

static bool g_enabled = false;

static uint32_t g_accum[ VS_BINS ][ VS_BINS ];
static uint32_t g_accum_max = 0;
static bool g_has_data      = false;

static uint8_t g_pixels[ VS_BINS ][ VS_BINS ][ 4 ];

// Маркеры hue (R, Y, G, C, B, M) в пространстве Cb/Cr — куда должны «притягиваться» пиксели
typedef struct
{
    point_s pos;         // координаты в пространстве Cb/Cr
    u8color_rgb_s color; // цвет маркера (для визуализации)
} hue_marker_t;

static const hue_marker_t g_hue_markers[ 6 ] = {
    {{85, 255},  {255, 0, 0}  }, // R
    {{1, 148},   {255, 255, 0}}, // Y
    {{43, 21},   {0, 255, 0}  }, // G
    {{171, 0},   {0, 255, 255}}, // C
    {{255, 107}, {0, 0, 255}  }, // B
    {{212, 235}, {255, 0, 255}}, // M
};

//------------------------------------------------------- API ---------------------------------------------------------

bool vectorscope_view_init(GLuint quad_vao)
{
    g_quad_vao = quad_vao;

    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, VS_BINS, VS_BINS, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    static const char * frag =
        "#version 330 core\n"
        "uniform sampler2D tex;\n"
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = texture(tex, TexCoord);\n"
        "}\n";

    g_program = gl_utils_build_program(frag);
    if (!g_program)
    {
        glDeleteTextures(1, &g_tex);
        g_tex = 0;
        return false;
    }

    glUseProgram(g_program);
    g_uniform_tex = glGetUniformLocation(g_program, "tex");
    return true;
}

void vectorscope_view_cleanup(void)
{
    if (g_program)
    {
        glDeleteProgram(g_program);
        g_program = 0;
    }
    if (g_tex)
    {
        glDeleteTextures(1, &g_tex);
        g_tex = 0;
    }
}

void vectorscope_view_update(const uint8_t * data, image_size_s size, const format_config_s * fmt,
                             const selection_s * sel, int bpp)
{
    if (!g_enabled || !data || !fmt)
    {
        return;
    }
    if (bpp < 3)
    {
        return;
    }

    memset(g_accum, 0, sizeof(g_accum));
    g_accum_max = 0;

    // ROI
    point_s start   = {0, 0};
    image_size_s sz = size;
    if (sel && selection_is_meaningful(sel))
    {
        rect_s r = selection_rect(sel);
        start.x  = utils_clamp(r.x0, 0, size.x);
        start.y  = utils_clamp(r.y0, 0, size.y);
        sz.x     = utils_clamp(r.x1, 0, size.x) - start.x;
        sz.y     = utils_clamp(r.y1, 0, size.y) - start.y;
    }

    if (sz.x <= 0 || sz.y <= 0)
    {
        return;
    }

    const bool is_ycbcr = fmt->is_ycbcr;
    const int step      = (bpp == 4) ? 4 : 3;

    for (int y = start.y; y < start.y + sz.y; y++)
    {
        const uint8_t * row = data + (size_t)y * (size_t)size.x * (size_t)step;
        for (int x = start.x; x < start.x + sz.x; x++)
        {
            const uint8_t * px = row + (size_t)x * (size_t)step;
            int cb, cr;

            if (is_ycbcr)
            {
                cb = px[ 1 ];
                cr = px[ 2 ];
            }
            else
            {
                const float r = (float)px[ 0 ];
                const float g = (float)px[ 1 ];
                const float b = (float)px[ 2 ];
                cb            = (int)(-0.169f * r - 0.331f * g + 0.500f * b + 128.0f);
                cr            = (int)(0.500f * r - 0.419f * g - 0.081f * b + 128.0f);
            }

            cb = utils_clamp(cb, 0, VS_BINS - 1);
            cr = utils_clamp(cr, 0, VS_BINS - 1);

            uint32_t v = ++g_accum[ cr ][ cb ];
            if (v > g_accum_max)
            {
                g_accum_max = v;
            }
        }
    }

    g_has_data = (g_accum_max > 0);

    // --- Построение текстуры ---
    memset(g_pixels, 0, sizeof(g_pixels));

    // 1. Фон берем из темы (UI_COLOR_BG_PANEL)
    fcolor_rgba_s bg = UI_COLOR_BG_PANEL;
    uint8_t bg_r     = (uint8_t)(bg.r * 255.0f);
    uint8_t bg_g     = (uint8_t)(bg.g * 255.0f);
    uint8_t bg_b     = (uint8_t)(bg.b * 255.0f);
    uint8_t bg_a     = (uint8_t)(bg.a * 255.0f);

    for (int y = 0; y < VS_BINS; y++)
    {
        for (int x = 0; x < VS_BINS; x++)
        {
            g_pixels[ y ][ x ][ 0 ] = bg_r;
            g_pixels[ y ][ x ][ 1 ] = bg_g;
            g_pixels[ y ][ x ][ 2 ] = bg_b;
            g_pixels[ y ][ x ][ 3 ] = bg_a;
        }
    }

    // 2. Данные — яркий Cyan с прозрачностью, зависящей от плотности
    if (g_has_data)
    {
        const float log_max = logf(1.0f + (float)g_accum_max);
        for (int y = 0; y < VS_BINS; y++)
        {
            for (int x = 0; x < VS_BINS; x++)
            {
                if (g_accum[ y ][ x ] == 0)
                {
                    continue;
                }

                // t от 0.0 (мало точек) до 1.0 (много точек)
                const float t = logf(1.0f + (float)g_accum[ y ][ x ]) / log_max;

                // Цвет акцента (Cyan из темы)
                fcolor_rgba_s accent = UI_COLOR_ACCENT_PRIMARY;

                g_pixels[ y ][ x ][ 0 ] = (uint8_t)(accent.r * 255.0f);
                g_pixels[ y ][ x ][ 1 ] = (uint8_t)(accent.g * 255.0f);
                g_pixels[ y ][ x ][ 2 ] = (uint8_t)(accent.b * 255.0f);

                // Делаем точки достаточно видимыми, даже если их мало
                g_pixels[ y ][ x ][ 3 ] = (uint8_t)(100.0f + t * 155.0f);
            }
        }
    }

    // 3. Оси по центру (делаем их чуть светлее, чтобы не сливались)
    for (int i = 0; i < VS_BINS; i++)
    {
        g_pixels[ 128 ][ i ][ 0 ] = 100;
        g_pixels[ 128 ][ i ][ 1 ] = 100;
        g_pixels[ 128 ][ i ][ 2 ] = 100;
        g_pixels[ 128 ][ i ][ 3 ] = 255;
        g_pixels[ i ][ 128 ][ 0 ] = 100;
        g_pixels[ i ][ 128 ][ 1 ] = 100;
        g_pixels[ i ][ 128 ][ 2 ] = 100;
        g_pixels[ i ][ 128 ][ 3 ] = 255;
    }

    // 4. Маркеры hue
    for (int i = 0; i < 6; i++)
    {
        const hue_marker_t * m = &g_hue_markers[ i ];
        if (m->pos.x >= 0 && m->pos.x < VS_BINS && m->pos.y >= 0 && m->pos.y < VS_BINS)
        {
            g_pixels[ m->pos.y ][ m->pos.x ][ 0 ] = m->color.r;
            g_pixels[ m->pos.y ][ m->pos.x ][ 1 ] = m->color.g;
            g_pixels[ m->pos.y ][ m->pos.x ][ 2 ] = m->color.b;
            g_pixels[ m->pos.y ][ m->pos.x ][ 3 ] = 255;
        }
    }

    glBindTexture(GL_TEXTURE_2D, g_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, VS_BINS, VS_BINS, GL_RGBA, GL_UNSIGNED_BYTE, g_pixels);
}

void vectorscope_view_render(image_size_s win, float dpi_scale)
{
    if (!g_enabled || !g_has_data || !g_program)
    {
        return;
    }

    const image_size_s view = {VS_VIEW, VS_VIEW};
    const int margin_x = (int)(10 * dpi_scale);
    const int margin_y = (int)(40 * dpi_scale);

    const point_s ov = {
        .x = utils_max(win.x - view.x - margin_x, 0),
        .y = margin_y,
    };

    glViewport(ov.x, ov.y, view.x, view.y);

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    if (g_uniform_tex >= 0)
    {
        glUniform1i(g_uniform_tex, 0);
    }

    glBindVertexArray(g_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void vectorscope_view_toggle_enabled(void)
{
    g_enabled = !g_enabled;
    log_info("Vectorscope %s", g_enabled ? "enabled" : "disabled");
}

bool vectorscope_view_is_enabled(void)
{
    return g_enabled;
}
