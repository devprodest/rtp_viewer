#include "histogram_view.h"
#include "analysis/analytics.h"
#include "log/logger.h"
#include "render/gl/gl_utils.h"
#include "ui_color_scheme.h"
#include "util/utils.h"

#include <math.h>
#include <string.h>

static GLuint g_quad_vao        = 0;
static GLuint g_hist_tex        = 0;
static GLuint g_hist_program    = 0;
static GLint g_hist_uniform_tex = -1;

static bool g_enabled   = false;
static bool g_log_scale = false;
static int g_eff_bits   = 8;

#define HIST_BAND_ROWS          (64)
#define HIST_TEX_H              (HIST_BAND_ROWS * 3)
#define HIST_BACKGROUND_OPACITY (20)
#define HIST_BIN_OPACITY        (200)

_Static_assert(HIST_TEX_H % 3 == 0, "HIST_TEX_H must be divisible by 3");

typedef uint8_t hist_pixel_t[ 4 ];

static union
{
    hist_pixel_t by_channel[ HIST_CHANNELS ][ HIST_BAND_ROWS ][ HIST_BINS ];
    hist_pixel_t by_row[ HIST_TEX_H ][ HIST_BINS ];
} g_hist_pixels;

//------------------------------------------------------- CDF (сглаживание) --------------------------------------------

/// \brief Простое скользящее среднее для сглаживания линии CDF.
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

static void draw_cdf_line_gray(const float * cdf_in, uint8_t r, uint8_t g, uint8_t b)
{
    float cdf[ HIST_BINS ];
    memcpy(cdf, cdf_in, sizeof(cdf));
    apply_smoothing(cdf, HIST_BINS);

    int prev_y = -1;

    for (int x = 0; x < HIST_BINS; x++)
    {
        int y_local = (int)(cdf[ x ] * (float)(HIST_TEX_H - 1) + 0.5f);
        int y_pix   = HIST_TEX_H - 1 - y_local;

        if (prev_y >= 0)
        {
            int ya = utils_min(prev_y, y_pix);
            int yb = utils_max(prev_y, y_pix);
            for (int y = ya; y <= yb; y++)
            {
                g_hist_pixels.by_row[ y ][ x ][ 0 ] = r;
                g_hist_pixels.by_row[ y ][ x ][ 1 ] = g;
                g_hist_pixels.by_row[ y ][ x ][ 2 ] = b;
                g_hist_pixels.by_row[ y ][ x ][ 3 ] = 255;
            }
        }
        prev_y = y_pix;
    }
}

static void draw_cdf_line_channel(const float * cdf_in, int band_index, int x_lo, int x_hi, int span, uint8_t r,
                                  uint8_t g, uint8_t b)
{
    float cdf[ HIST_BINS ];
    memcpy(cdf, cdf_in, sizeof(cdf));
    apply_smoothing(cdf, HIST_BINS);

    int y0     = band_index * HIST_BAND_ROWS;
    int prev_y = -1;

    for (int x = x_lo; x <= x_hi; x++)
    {
        int xs = utils_clamp((x - x_lo) * HIST_BINS / span, 0, HIST_BINS - 1);

        int y_local = (int)(cdf[ x ] * (float)(HIST_BAND_ROWS - 1) + 0.5f);
        int y_pix   = y0 + (HIST_BAND_ROWS - 1 - y_local);

        if (prev_y >= 0)
        {
            int ya = utils_min(prev_y, y_pix);
            int yb = utils_max(prev_y, y_pix);
            for (int y = ya; y <= yb; y++)
            {
                if (y >= y0 && y < y0 + HIST_BAND_ROWS)
                {
                    g_hist_pixels.by_channel[ band_index ][ y - y0 ][ xs ][ 0 ] = r;
                    g_hist_pixels.by_channel[ band_index ][ y - y0 ][ xs ][ 1 ] = g;
                    g_hist_pixels.by_channel[ band_index ][ y - y0 ][ xs ][ 2 ] = b;
                    g_hist_pixels.by_channel[ band_index ][ y - y0 ][ xs ][ 3 ] = 255;
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

/// \brief Рисует вертикальные линии на позициях min и max из аналитики.
//------------------------------------------------------- Указатели краёв
//-----------------------------------------------

/// \brief Рисует вертикальные линии на позициях перцентилей 1% и 99%, используя готовую CDF.
static void draw_edge_pointers(const float * cdf, int channel, int lo, int span)
{
    if (!cdf)
    {
        return;
    }

    int min_bin = 0;
    int max_bin = HIST_BINS - 1;

    // Находим бины для 1% и 99% по уже готовой CDF
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

    // Переводим в координаты X на текстуре
    int x_min = utils_clamp((min_bin - lo) * HIST_BINS / span, 0, HIST_BINS - 1);
    int x_max = utils_clamp((max_bin - lo) * HIST_BINS / span, 0, HIST_BINS - 1);

    int y0 = (channel == -1) ? 0 : (channel * HIST_BAND_ROWS);
    int h  = (channel == -1) ? HIST_TEX_H : HIST_BAND_ROWS;

    uint8_t r_min = (uint8_t)(UI_HIST_EDGE_MIN.r * 255.0f);
    uint8_t g_min = (uint8_t)(UI_HIST_EDGE_MIN.g * 255.0f);
    uint8_t b_min = (uint8_t)(UI_HIST_EDGE_MIN.b * 255.0f);

    uint8_t r_max = (uint8_t)(UI_HIST_EDGE_MAX.r * 255.0f);
    uint8_t g_max = (uint8_t)(UI_HIST_EDGE_MAX.g * 255.0f);
    uint8_t b_max = (uint8_t)(UI_HIST_EDGE_MAX.b * 255.0f);

    for (int y = 0; y < h; y++)
    {
        // Ширина 1 пиксель
        int xm = utils_clamp(x_min, 0, HIST_BINS - 1);
        int xM = utils_clamp(x_max, 0, HIST_BINS - 1);

        // Минимум (1%)
        g_hist_pixels.by_row[ y0 + y ][ xm ][ 0 ] = r_min;
        g_hist_pixels.by_row[ y0 + y ][ xm ][ 1 ] = g_min;
        g_hist_pixels.by_row[ y0 + y ][ xm ][ 2 ] = b_min;
        g_hist_pixels.by_row[ y0 + y ][ xm ][ 3 ] = 255;

        // Максимум (99%)
        g_hist_pixels.by_row[ y0 + y ][ xM ][ 0 ] = r_max;
        g_hist_pixels.by_row[ y0 + y ][ xM ][ 1 ] = g_max;
        g_hist_pixels.by_row[ y0 + y ][ xM ][ 2 ] = b_max;
        g_hist_pixels.by_row[ y0 + y ][ xM ][ 3 ] = 255;
    }
}


//------------------------------------------------------- Основная сборка ----------------------------------------------

/// \brief Заполняет g_hist_pixels растр по текущей гистограмме из analytics.
static void build_histogram_image(void)
{
    const histogram_s * hist = analytics_get_histogram();

    memset(&g_hist_pixels, 0, sizeof(g_hist_pixels));

    const bool is_ycbcr = analytics_format_is_ycbcr();

    if (hist->active_channels == 1)
    {
        for (size_t y = 0; y < HIST_TEX_H; y++)
        {
            for (size_t x = 0; x < HIST_BINS; x++)
            {
                g_hist_pixels.by_row[ y ][ x ][ 0 ] = 255;
                g_hist_pixels.by_row[ y ][ x ][ 1 ] = 255;
                g_hist_pixels.by_row[ y ][ x ][ 2 ] = 255;
                g_hist_pixels.by_row[ y ][ x ][ 3 ] = HIST_BACKGROUND_OPACITY;
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
                    bar = (int)(log(1.0 + (double)hist->bins[ 0 ][ x ]) / log(1.0 + (double)maxv) * HIST_TEX_H);
                }
                else
                {
                    bar = (int)((uint64_t)hist->bins[ 0 ][ x ] * HIST_TEX_H / maxv);
                }

                for (int y = 0; y < bar; y++)
                {
                    g_hist_pixels.by_row[ HIST_TEX_H - 1 - y ][ x ][ 3 ] = HIST_BIN_OPACITY;
                }
            }

            // CDF
            float cdf[ HIST_BINS ];
            compute_cdf_channel(hist->bins[ 0 ], cdf);
            draw_cdf_line_gray(cdf, 255, 255, 0); // жёлтая линия CDF

            // Указатели краёв (рисуем в самом конце)
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
                    g_hist_pixels.by_channel[ c ][ y ][ x ][ 0 ] = (*color)[ c ][ 0 ];
                    g_hist_pixels.by_channel[ c ][ y ][ x ][ 1 ] = (*color)[ c ][ 1 ];
                    g_hist_pixels.by_channel[ c ][ y ][ x ][ 2 ] = (*color)[ c ][ 2 ];
                    g_hist_pixels.by_channel[ c ][ y ][ x ][ 3 ] = HIST_BACKGROUND_OPACITY;
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
                    g_hist_pixels.by_channel[ c ][ HIST_BAND_ROWS - 1 - y ][ xs ][ 3 ] = HIST_BIN_OPACITY;
                }
            }

            // CDF
            float cdf[ HIST_BINS ];
            compute_cdf_channel(hist->bins[ c ], cdf);
            draw_cdf_line_channel(cdf, c, lo, hi, span, 255, 255, 0);

            // Указатели краёв (рисуем в самом конце)
            draw_edge_pointers(cdf, c, lo, span);
        }
    }

    glBindTexture(GL_TEXTURE_2D, g_hist_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HIST_BINS, HIST_TEX_H, GL_RGBA, GL_UNSIGNED_BYTE, &g_hist_pixels);
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
        "in vec2 TexCoord;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = texture(tex, TexCoord);\n"
        "}\n";

    g_hist_program = gl_utils_build_program(hist_frag);
    if (!g_hist_program)
    {
        glDeleteTextures(1, &g_hist_tex);
        g_hist_tex = 0;
        return false;
    }

    glUseProgram(g_hist_program);
    g_hist_uniform_tex = glGetUniformLocation(g_hist_program, "tex");

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

void histogram_view_render(image_size_s win)
{
    const histogram_s * hist = analytics_get_histogram();

    if (!g_enabled || hist->active_channels == 0 || !g_hist_program)
    {
        return;
    }

    const image_size_s hist_view_size = {320, 240};

    const int margin = 10;
    const point_s ov = {.x = utils_max(win.x - hist_view_size.x - margin, 0),
                        .y = utils_max(win.y - hist_view_size.y - margin, 0)};

    glViewport(ov.x, ov.y, hist_view_size.x, hist_view_size.y);

    glUseProgram(g_hist_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_hist_tex);
    if (g_hist_uniform_tex >= 0)
    {
        glUniform1i(g_hist_uniform_tex, 0);
    }

    glBindVertexArray(g_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void histogram_view_toggle_enabled(void)
{
    g_enabled = !g_enabled;
    log_info("Histogram %s", g_enabled ? "enabled" : "disabled");
}

void histogram_view_toggle_log_scale(void)
{
    g_log_scale = !g_log_scale;
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
