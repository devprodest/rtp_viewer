#include "histogram_view.h"
#include "analysis/analytics.h"
#include "app/settings.h"
#include "log/logger.h"
#include "panel.h"
#include "render/gl/gl_utils.h"
#include "ui_color_scheme.h"
#include "util/utils.h"

#include <math.h>
#include <string.h>

static GLuint g_quad_vao               = 0;
static GLuint g_hist_tex               = 0;
static GLuint g_hist_program           = 0;
static GLint g_hist_uniform_tex        = -1;
static GLint g_hist_uniform_uv_scale_y = -1;

static point_s g_pos    = {-1, -1};
static bool g_enabled   = false;
static bool g_log_scale = false;
static int g_eff_bits   = 8;

#define HIST_BAND_ROWS          (64)
#define HIST_TEX_H              (HIST_BAND_ROWS * HIST_CHANNELS)
#define HIST_BACKGROUND_OPACITY (20)
#define HIST_BIN_OPACITY        (200)

static_assert(HIST_TEX_H % HIST_CHANNELS == 0, "HIST_TEX_H must be divisible by HIST_CHANNELS");

typedef uint8_t hist_pixel_t[ 4 ];

/// \brief Прямоугольная текстура HIST_BINS × HIST_TEX_H в памяти как
///        [channel][row][bin][rgba]. Для grayscale заполняется только [0],
///        для RGB/YCbCr — все три канала.
static hist_pixel_t g_hist_pixels[ HIST_CHANNELS ][ HIST_BAND_ROWS ][ HIST_BINS ];

//------------------------------------------------------- CDF (сглаживание) --------------------------------------------

static void apply_smoothing(float * data, int size)
{
    if (size < 3)
    {
        return;
    }

    float temp[ HIST_BINS ];
    temp[ 0 ] = data[ 0 ];
    for (int i = 1; i < size - 1; i++)
    {
        temp[ i ] = (data[ i - 1 ] + data[ i ] * 2.0f + data[ i + 1 ]) / 4.0f;
    }
    temp[ size - 1 ] = data[ size - 1 ];
    memcpy(data, temp, size * sizeof(float));
}

static void draw_cdf_line_gray(const float * cdf_in)
{
    float cdf[ HIST_BINS ];
    memcpy(cdf, cdf_in, sizeof(cdf));
    apply_smoothing(cdf, HIST_BINS);

    int prev_y = -1;

    for (int x = 0; x < HIST_BINS; x++)
    {
        int y_local = (int)(cdf[ x ] * (float)(HIST_BAND_ROWS - 1) + 0.5f);
        int y_pix   = HIST_BAND_ROWS - 1 - y_local;

        if (prev_y >= 0)
        {
            int ya = utils_min(prev_y, y_pix);
            int yb = utils_max(prev_y, y_pix);
            for (int y = ya; y <= yb; y++)
            {
                g_hist_pixels[ 0 ][ y ][ x ][ 0 ] = 255;
                g_hist_pixels[ 0 ][ y ][ x ][ 1 ] = 255;
                g_hist_pixels[ 0 ][ y ][ x ][ 2 ] = 0;
                g_hist_pixels[ 0 ][ y ][ x ][ 3 ] = 255;
            }
        }
        prev_y = y_pix;
    }
}

static void draw_cdf_line_channel(const float * cdf_in, int channel, int x_lo, int x_hi, int span)
{
    float cdf[ HIST_BINS ];
    memcpy(cdf, cdf_in, sizeof(cdf));
    apply_smoothing(cdf, HIST_BINS);

    int prev_y = -1;

    for (int x = x_lo; x <= x_hi; x++)
    {
        int xs = utils_clamp((x - x_lo) * HIST_BINS / span, 0, HIST_BINS - 1);

        int y_local = (int)(cdf[ x ] * (float)(HIST_BAND_ROWS - 1) + 0.5f);
        int y_pix   = HIST_BAND_ROWS - 1 - y_local;

        if (prev_y >= 0)
        {
            int ya = utils_min(prev_y, y_pix);
            int yb = utils_max(prev_y, y_pix);
            for (int y = ya; y <= yb; y++)
            {
                if (y >= 0 && y < HIST_BAND_ROWS)
                {
                    g_hist_pixels[ channel ][ y ][ xs ][ 0 ] = 255;
                    g_hist_pixels[ channel ][ y ][ xs ][ 1 ] = 255;
                    g_hist_pixels[ channel ][ y ][ xs ][ 2 ] = 0;
                    g_hist_pixels[ channel ][ y ][ xs ][ 3 ] = 255;
                }
            }
        }
        prev_y = y_pix;
    }
}

static void compute_cdf_channel(const uint32_t * bins, float * cdf_out)
{
    uint64_t total = 0;
    for (int i = 0; i < HIST_BINS; i++)
    {
        total += bins[ i ];
    }
    if (total == 0)
    {
        for (int i = 0; i < HIST_BINS; i++)
        {
            cdf_out[ i ] = 0.0f;
        }
        return;
    }

    uint64_t acc = 0;
    for (int i = 0; i < HIST_BINS; i++)
    {
        acc += bins[ i ];
        cdf_out[ i ] = (float)acc / (float)total;
    }
}

//------------------------------------------------------- Указатели краёв
//-----------------------------------------------

static void draw_edge_pointers(const float * cdf, int channel, int lo, int span)
{
    if (!cdf || channel < 0 || channel >= HIST_CHANNELS)
    {
        return;
    }

    int min_bin = 0;
    int max_bin = HIST_BINS - 1;

    for (int i = 0; i < HIST_BINS; i++)
    {
        if (cdf[ i ] >= 0.01f)
        {
            min_bin = i;
            break;
        }
    }
    for (int i = 0; i < HIST_BINS; i++)
    {
        if (cdf[ i ] >= 0.99f)
        {
            max_bin = i;
            break;
        }
    }

    int x_min = utils_clamp((min_bin - lo) * HIST_BINS / span, 0, HIST_BINS - 1);
    int x_max = utils_clamp((max_bin - lo) * HIST_BINS / span, 0, HIST_BINS - 1);

    uint8_t r_min = (uint8_t)(UI_HIST_EDGE_MIN.r * 255.0f);
    uint8_t g_min = (uint8_t)(UI_HIST_EDGE_MIN.g * 255.0f);
    uint8_t b_min = (uint8_t)(UI_HIST_EDGE_MIN.b * 255.0f);

    uint8_t r_max = (uint8_t)(UI_HIST_EDGE_MAX.r * 255.0f);
    uint8_t g_max = (uint8_t)(UI_HIST_EDGE_MAX.g * 255.0f);
    uint8_t b_max = (uint8_t)(UI_HIST_EDGE_MAX.b * 255.0f);

    for (int y = 0; y < HIST_BAND_ROWS; y++)
    {
        g_hist_pixels[ channel ][ y ][ x_min ][ 0 ] = r_min;
        g_hist_pixels[ channel ][ y ][ x_min ][ 1 ] = g_min;
        g_hist_pixels[ channel ][ y ][ x_min ][ 2 ] = b_min;
        g_hist_pixels[ channel ][ y ][ x_min ][ 3 ] = 255;

        g_hist_pixels[ channel ][ y ][ x_max ][ 0 ] = r_max;
        g_hist_pixels[ channel ][ y ][ x_max ][ 1 ] = g_max;
        g_hist_pixels[ channel ][ y ][ x_max ][ 2 ] = b_max;
        g_hist_pixels[ channel ][ y ][ x_max ][ 3 ] = 255;
    }
}

//------------------------------------------------------- Основная сборка ----------------------------------------------

static void build_histogram_image(void)
{
    const histogram_s * hist = analytics_get_histogram();

    memset(&g_hist_pixels, 0, sizeof(g_hist_pixels));

    const bool is_ycbcr = analytics_format_is_ycbcr();

    if (hist->active_channels == 1)
    {
        // Grayscale: только канал 0 (64 строки).
        for (size_t y = 0; y < HIST_BAND_ROWS; y++)
        {
            for (size_t x = 0; x < HIST_BINS; x++)
            {
                g_hist_pixels[ 0 ][ y ][ x ][ 0 ] = 255;
                g_hist_pixels[ 0 ][ y ][ x ][ 1 ] = 255;
                g_hist_pixels[ 0 ][ y ][ x ][ 2 ] = 255;
                g_hist_pixels[ 0 ][ y ][ x ][ 3 ] = HIST_BACKGROUND_OPACITY;
            }
        }

        const uint32_t maxv = hist->max_per_channel[ 0 ];
        if (maxv > 0)
        {
            for (int x = 0; x < HIST_BINS; x++)
            {
                int bar;
                if (g_log_scale)
                {
                    bar = (int)(log(1.0 + (double)hist->bins[ 0 ][ x ]) / log(1.0 + (double)maxv) * HIST_BAND_ROWS);
                }
                else
                {
                    bar = (int)((uint64_t)hist->bins[ 0 ][ x ] * HIST_BAND_ROWS / maxv);
                }

                for (int y = 0; y < bar; y++)
                {
                    g_hist_pixels[ 0 ][ HIST_BAND_ROWS - 1 - y ][ x ][ 3 ] = HIST_BIN_OPACITY;
                }
            }

            float cdf[ HIST_BINS ];
            compute_cdf_channel(hist->bins[ 0 ], cdf);
            draw_cdf_line_gray(cdf);
            draw_edge_pointers(cdf, 0, 0, HIST_BINS);
        }
    }
    else if (hist->active_channels == 3)
    {
        static const uint8_t col_rgb[ 3 ][ 3 ] = {
            {255, 0,   0  },
            {0,   255, 0  },
            {0,   0,   255},
        };
        static const uint8_t col_ycbcr[ 3 ][ 3 ] = {
            {220, 220, 220},
            {80,  120, 255},
            {255, 80,  80 },
        };
        const uint8_t (*color)[ 3 ][ 3 ] = is_ycbcr ? &col_ycbcr : &col_rgb;

        for (size_t c = 0; c < 3; c++)
        {
            for (size_t y = 0; y < HIST_BAND_ROWS; y++)
            {
                for (size_t x = 0; x < HIST_BINS; x++)
                {
                    g_hist_pixels[ c ][ y ][ x ][ 0 ] = (*color)[ c ][ 0 ];
                    g_hist_pixels[ c ][ y ][ x ][ 1 ] = (*color)[ c ][ 1 ];
                    g_hist_pixels[ c ][ y ][ x ][ 2 ] = (*color)[ c ][ 2 ];
                    g_hist_pixels[ c ][ y ][ x ][ 3 ] = HIST_BACKGROUND_OPACITY;
                }
            }
        }

        for (int c = 0; c < 3; c++)
        {
            const bool use_window = is_ycbcr && c != 0;
            const int lo          = use_window ? 96 : 0;
            const int hi          = use_window ? 160 : (HIST_BINS - 1);
            const int span        = hi - lo + 1;

            const uint32_t maxv = hist->max_per_channel[ c ];
            if (maxv == 0)
            {
                continue;
            }

            for (int x = lo; x <= hi; x++)
            {
                int bar;
                if (g_log_scale)
                {
                    bar = (int)(log(1.0 + (double)hist->bins[ c ][ x ]) / log(1.0 + (double)maxv) * HIST_BAND_ROWS);
                }
                else
                {
                    bar = (int)((uint64_t)hist->bins[ c ][ x ] * HIST_BAND_ROWS / maxv);
                }

                int xs = utils_clamp((x - lo) * HIST_BINS / span, 0, HIST_BINS - 1);

                for (int y = 0; y < bar; y++)
                {
                    g_hist_pixels[ c ][ HIST_BAND_ROWS - 1 - y ][ xs ][ 3 ] = HIST_BIN_OPACITY;
                }
            }

            float cdf[ HIST_BINS ];
            compute_cdf_channel(hist->bins[ c ], cdf);
            draw_cdf_line_channel(cdf, c, lo, hi, span);
            draw_edge_pointers(cdf, c, lo, span);
        }
    }

    glBindTexture(GL_TEXTURE_2D, g_hist_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HIST_BINS, HIST_TEX_H, GL_RGBA, GL_UNSIGNED_BYTE, g_hist_pixels);
}

//------------------------------------------------------- API ---------------------------------------------------------

bool histogram_view_init(GLuint quad_vao)
{
    g_quad_vao = quad_vao;

    glGenTextures(1, &g_hist_tex);
    glBindTexture(GL_TEXTURE_2D, g_hist_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, HIST_BINS, HIST_TEX_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    static const char * hist_frag =
        "#version 330 core\n"
        "uniform sampler2D tex;\n"
        "uniform float uv_scale_y;\n"
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = texture(tex, vec2(TexCoord.x, TexCoord.y * uv_scale_y));\n"
        "}\n";

    g_hist_program = gl_utils_build_program(hist_frag);
    if (!g_hist_program)
    {
        glDeleteTextures(1, &g_hist_tex);
        g_hist_tex = 0;
        return false;
    }

    glUseProgram(g_hist_program);
    g_hist_uniform_tex        = glGetUniformLocation(g_hist_program, "tex");
    g_hist_uniform_uv_scale_y = glGetUniformLocation(g_hist_program, "uv_scale_y");

    bool ls;
    if (settings_get_feature_bool("histogram", "log_scale", &ls))
    {
        g_log_scale = ls;
    }
    log_info("histogram: log_scale = %d", g_log_scale ? 1 : 0);

    return true;
}

void histogram_view_cleanup(void)
{
    if (g_hist_program)
    {
        glDeleteProgram(g_hist_program);
        g_hist_program = 0;
    }
    if (g_hist_tex)
    {
        glDeleteTextures(1, &g_hist_tex);
        g_hist_tex = 0;
    }
}

void histogram_view_update(int effective_bits)
{
    if (!g_enabled)
    {
        return;
    }
    g_eff_bits = effective_bits;
    build_histogram_image();
}

void histogram_view_render(point_s pos, image_size_s win, float dpi_scale)
{
    const histogram_s * hist = analytics_get_histogram();

    if (!g_enabled || hist->active_channels == 0 || !g_hist_program)
    {
        return;
    }

    const image_size_s sz = histogram_view_get_size();

    if (pos.x < 0 || pos.y < 0)
    {
        pos   = (point_s) {utils_max(win.x - sz.x - 10, 0), 10};
        g_pos = pos;
    }

    int title_h   = panel_title_height(dpi_scale);
    int content_y = pos.y + title_h;
    int gl_y      = win.y - content_y - sz.y;

    glViewport(pos.x, gl_y, sz.x, sz.y);
    glUseProgram(g_hist_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_hist_tex);
    if (g_hist_uniform_tex >= 0)
    {
        glUniform1i(g_hist_uniform_tex, 0);
    }
    if (g_hist_uniform_uv_scale_y >= 0)
    {
        const float uv_y = (hist->active_channels == 1) ? (float)HIST_BAND_ROWS / (float)HIST_TEX_H : 1.0f;
        glUniform1f(g_hist_uniform_uv_scale_y, uv_y);
    }

    glBindVertexArray(g_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void histogram_view_disable(void)
{
    if (!g_enabled)
    {
        return;
    }
    g_enabled   = false;
    g_log_scale = false;
    settings_set_panel_enabled("histogram", false);
    settings_set_feature_bool("histogram", "log_scale", false);
    log_info("Histogram disabled");
}

void histogram_view_toggle_enabled(void)
{
    g_enabled = !g_enabled;
    settings_set_panel_enabled("histogram", g_enabled);
    log_info("Histogram %s", g_enabled ? "enabled" : "disabled");
}

void histogram_view_toggle_log_scale(void)
{
    g_log_scale = !g_log_scale;
    settings_set_feature_bool("histogram", "log_scale", g_log_scale);
    log_info("Histogram scale: %s", g_log_scale ? "log" : "linear");
}

bool histogram_view_is_enabled(void)
{
    return g_enabled;
}

int histogram_view_get_active_channels(void)
{
    return analytics_get_histogram()->active_channels;
}

bool histogram_view_is_log_scale(void)
{
    return g_log_scale;
}

point_s histogram_view_get_pos(void)
{
    return g_pos;
}

void histogram_view_set_pos(point_s pos)
{
    g_pos = pos;
}

image_size_s histogram_view_get_size(void)
{
    const histogram_s * hist = analytics_get_histogram();
    // Grayscale: одна полоса из трёх → панель в 3 раза ниже.
    const int h = (hist->active_channels == 1) ? 240 * HIST_BAND_ROWS / HIST_TEX_H : 240;
    return (image_size_s) {320, h};
}
