#include "histogram_view.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "analysis/histogram.h"
#include "gl_utils.h"

static GLuint g_quad_vao     = 0;
static GLuint g_hist_tex     = 0;
static GLuint g_hist_program = 0;
static GLint  g_hist_uniform_tex = -1;

static histogram_s g_histogram;

static bool g_enabled     = false;
static bool g_log_scale   = false;
static bool g_cdf_enabled = false;

static int  g_width  = 0;
static int  g_height = 0;
static int  g_gl_format = 0;
static int  g_gl_type   = 0;

static char g_format_name[64] = {0};

#define HIST_BAND_ROWS          (64)
#define HIST_TEX_H              (HIST_BAND_ROWS * 3)
#define HIST_BACKGROUND_OPACITY (20)
#define HIST_BIN_OPACITY        (255)

_Static_assert(HIST_TEX_H % 3 == 0, "HIST_TEX_H must be divisible by 3");

typedef uint8_t hist_pixel_t[4];

static union
{
    hist_pixel_t by_channel[HIST_CHANNELS][HIST_BAND_ROWS][HIST_BINS];
    hist_pixel_t by_row[HIST_TEX_H][HIST_BINS];
} g_hist_pixels;

/// \brief Является ли текущий формат YCbCr.
static bool hist_format_is_ycbcr(void)
{
    return g_format_name[0] != '\0' && strcmp(g_format_name, "ycbcr") == 0;
}

static void draw_cdf_line_gray(const float * cdf, uint8_t r, uint8_t g, uint8_t b)
{
    int prev_y = -1;

    for (int x = 0; x < HIST_BINS; x++)
    {
        int y_local = (int)(cdf[x] * (float)(HIST_TEX_H - 1) + 0.5f);
        int y_pix   = HIST_TEX_H - 1 - y_local;

        if (prev_y >= 0)
        {
            int ya = prev_y < y_pix ? prev_y : y_pix;
            int yb = prev_y > y_pix ? prev_y : y_pix;
            for (int y = ya; y <= yb; y++)
            {
                g_hist_pixels.by_row[y][x][0] = r;
                g_hist_pixels.by_row[y][x][1] = g;
                g_hist_pixels.by_row[y][x][2] = b;
                g_hist_pixels.by_row[y][x][3] = 255;
            }
        }
        prev_y = y_pix;
    }
}

static void draw_cdf_line_channel(const float * cdf, int band_index, int x_lo, int x_hi, int span, uint8_t r, uint8_t g,
                                  uint8_t b)
{
    int y0     = band_index * HIST_BAND_ROWS;
    int prev_y = -1;

    for (int x = x_lo; x <= x_hi; x++)
    {
        int xs = (x - x_lo) * HIST_BINS / span;
        if (xs >= HIST_BINS)
        {
            xs = HIST_BINS - 1;
        }

        int y_local = (int)(cdf[x] * (float)(HIST_BAND_ROWS - 1) + 0.5f);
        int y_pix   = y0 + (HIST_BAND_ROWS - 1 - y_local);

        if (prev_y >= 0)
        {
            int ya = prev_y < y_pix ? prev_y : y_pix;
            int yb = prev_y > y_pix ? prev_y : y_pix;
            for (int y = ya; y <= yb; y++)
            {
                if (y >= y0 && y < y0 + HIST_BAND_ROWS)
                {
                    g_hist_pixels.by_channel[band_index][y - y0][xs][0] = r;
                    g_hist_pixels.by_channel[band_index][y - y0][xs][1] = g;
                    g_hist_pixels.by_channel[band_index][y - y0][xs][2] = b;
                    g_hist_pixels.by_channel[band_index][y - y0][xs][3] = 255;
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
        total += bins[i];
    }
    if (total == 0)
    {
        for (int i = 0; i < HIST_BINS; i++)
        {
            cdf_out[i] = 0.0f;
        }
        return;
    }

    uint64_t acc = 0;
    for (int i = 0; i < HIST_BINS; i++)
    {
        acc += bins[i];
        cdf_out[i] = (float)acc / (float)total;
    }
}

static void build_histogram_image(void)
{
    memset(&g_hist_pixels, 0, sizeof(g_hist_pixels));

    if (g_histogram.active_channels == 1)
    {
        for (size_t y = 0; y < HIST_TEX_H; y++)
        {
            for (size_t x = 0; x < HIST_BINS; x++)
            {
                g_hist_pixels.by_row[y][x][0] = 255;
                g_hist_pixels.by_row[y][x][1] = 255;
                g_hist_pixels.by_row[y][x][2] = 255;
                g_hist_pixels.by_row[y][x][3] = HIST_BACKGROUND_OPACITY;
            }
        }

        const uint32_t maxv = g_histogram.max_per_channel[0];
        if (maxv > 0)
        {
            for (int x = 0; x < HIST_BINS; x++)
            {
                int bar;
                if (g_log_scale)
                {
                    bar = (int)(log(1.0 + (double)g_histogram.bins[0][x]) / log(1.0 + (double)maxv) * HIST_TEX_H);
                }
                else
                {
                    bar = (int)((uint64_t)g_histogram.bins[0][x] * HIST_TEX_H / maxv);
                }

                for (int y = 0; y < bar; y++)
                {
                    g_hist_pixels.by_row[HIST_TEX_H - 1 - y][x][3] = HIST_BIN_OPACITY;
                }
            }
        }

        if (g_cdf_enabled && maxv > 0)
        {
            float cdf[HIST_BINS];
            compute_cdf_channel(g_histogram.bins[0], cdf);
            draw_cdf_line_gray(cdf, 255, 255, 0);
        }
    }
    else if (g_histogram.active_channels == 3)
    {
        static const uint8_t col_rgb[3][3] = {
            {255, 0,   0  },
            {0,   255, 0  },
            {0,   0,   255},
        };
        static const uint8_t col_ycbcr[3][3] = {
            {220, 220, 220},
            {80,  120, 255},
            {255, 80,  80 },
        };
        const bool is_ycbcr = hist_format_is_ycbcr();

        const uint8_t (*color)[3][3] = is_ycbcr ? &col_ycbcr : &col_rgb;

        for (size_t c = 0; c < 3; c++)
        {
            for (size_t y = 0; y < HIST_BAND_ROWS; y++)
            {
                for (size_t x = 0; x < HIST_BINS; x++)
                {
                    g_hist_pixels.by_channel[c][y][x][0] = (*color)[c][0];
                    g_hist_pixels.by_channel[c][y][x][1] = (*color)[c][1];
                    g_hist_pixels.by_channel[c][y][x][2] = (*color)[c][2];
                    g_hist_pixels.by_channel[c][y][x][3] = HIST_BACKGROUND_OPACITY;
                }
            }
        }

        for (int c = 0; c < 3; c++)
        {
            const bool use_window = is_ycbcr && c != 0;
            const int lo          = use_window ? 96 : 0;
            const int hi          = use_window ? 160 : (HIST_BINS - 1);
            const int span        = hi - lo + 1;

            const uint32_t maxv = g_histogram.max_per_channel[c];
            if (maxv == 0)
            {
                continue;
            }

            for (int x = lo; x <= hi; x++)
            {
                int bar;
                if (g_log_scale)
                {
                    bar = (int)(log(1.0 + (double)g_histogram.bins[c][x]) / log(1.0 + (double)maxv) * HIST_BAND_ROWS);
                }
                else
                {
                    bar = (int)((uint64_t)g_histogram.bins[c][x] * HIST_BAND_ROWS / maxv);
                }

                int xs = (x - lo) * HIST_BINS / span;

                for (int y = 0; y < bar; y++)
                {
                    g_hist_pixels.by_channel[c][HIST_BAND_ROWS - 1 - y][xs][3] = HIST_BIN_OPACITY;
                }
            }
        }

        if (g_cdf_enabled)
        {
            static const uint8_t cdf_colors[3][3] = {
                {255, 255, 0  },
                {0,   255, 255},
                {255, 0,   255},
            };

            for (int c = 0; c < 3; c++)
            {
                float cdf[HIST_BINS];
                compute_cdf_channel(g_histogram.bins[c], cdf);

                const bool use_window = is_ycbcr && c != 0;
                const int lo          = use_window ? 96 : 0;
                const int hi          = use_window ? 160 : (HIST_BINS - 1);
                const int span        = hi - lo + 1;

                draw_cdf_line_channel(cdf, c, lo, hi, span, cdf_colors[c][0], cdf_colors[c][1], cdf_colors[c][2]);
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, g_hist_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HIST_BINS, HIST_TEX_H, GL_RGBA, GL_UNSIGNED_BYTE, &g_hist_pixels);
}

static void compute_histogram(const uint8_t * data)
{
    histogram_reset(&g_histogram);

    const size_t n = (size_t)g_width * (size_t)g_height;

    if (g_gl_format == GL_RED && g_gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 1;
        for (size_t i = 0; i < n; i++)
        {
            histogram_add(&g_histogram, 0, data[i]);
        }
    }
    else if (g_gl_format == GL_RED && g_gl_type == GL_UNSIGNED_SHORT)
    {
        g_histogram.active_channels = 1;
        const uint16_t * p          = (const uint16_t *)data;
        for (size_t i = 0; i < n; i++)
        {
            histogram_add(&g_histogram, 0, p[i] >> 8);
        }
    }
    else if ((g_gl_format == GL_RGB || g_gl_format == GL_RGBA) && g_gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 3;
        const int step              = (g_gl_format == GL_RGBA) ? 4 : 3;
        for (size_t i = 0; i < n; i++)
        {
            const uint8_t * px = data + (i * (size_t)step);
            histogram_add(&g_histogram, 0, px[0]);
            histogram_add(&g_histogram, 1, px[1]);
            histogram_add(&g_histogram, 2, px[2]);
        }
    }
    else
    {
        g_histogram.active_channels = 0;
    }

    histogram_finalize(&g_histogram);
}

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

void histogram_view_update(const uint8_t * data, int width, int height, const format_config_s * fmt,
                           const char * format_name)
{
    g_width     = width;
    g_height    = height;
    g_gl_format = fmt->gl_format;
    g_gl_type   = fmt->gl_type;

    snprintf(g_format_name, sizeof(g_format_name), "%s", format_name ? format_name : "");

    compute_histogram(data);
    build_histogram_image();
}

void histogram_view_render(int x, int y, int w, int h)
{
    if (!g_enabled || g_histogram.active_channels == 0 || !g_hist_program)
    {
        return;
    }

    glViewport(x, y, w, h);

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
    printf("Histogram %s\n", g_enabled ? "enabled" : "disabled");
}

void histogram_view_toggle_log_scale(void)
{
    g_log_scale = !g_log_scale;
    printf("Histogram scale: %s\n", g_log_scale ? "log" : "linear");
}

void histogram_view_toggle_cdf(void)
{
    g_cdf_enabled = !g_cdf_enabled;
    printf("Histogram CDF %s\n", g_cdf_enabled ? "enabled" : "disabled");
}

bool histogram_view_is_enabled(void)
{
    return g_enabled;
}

int histogram_view_get_active_channels(void)
{
    return g_histogram.active_channels;
}