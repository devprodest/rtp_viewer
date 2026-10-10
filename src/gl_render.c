
#include "gl_render.h"
#include "histogram.h"
#include "png_writer.h"
#include "saver.h"
#include "stats.h"
#include "text_overlay.h"
#include "utils.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLFWwindow * g_window          = nullptr;
static GLuint g_texture               = 0;
static GLuint g_program               = 0;
static GLint g_uniform_tex            = -1;
static GLint g_uniform_width          = -1;
static GLint g_uniform_height         = -1;
static GLint g_uniform_effective_bits = -1;
static GLint g_uniform_demosaic_mode  = -1;
static int g_width                    = 0;
static int g_height                   = 0;
static int g_eff_bits                 = 16;
static bool g_smoothing               = false;
static int g_demosaic_mode            = 0;
static bool g_screenshot_requested    = false;
static bool g_paused                  = false;

static GLuint g_vao      = 0;
static GLuint g_shot_fbo = 0;
static GLuint g_shot_tex = 0;

// Гистограммы
static histogram_s g_histogram;
static bool g_histogram_log_scale   = false; ///< true — логарифмическая шкала, false — линейная
static bool g_histogram_enabled     = false;
static bool g_histogram_cdf_enabled = false;
static GLuint g_hist_tex            = 0;
static GLuint g_hist_program        = 0;
static GLint g_hist_uniform_tex     = -1;

static GLuint g_format_fbo             = 0; ///< FBO с результатом форматного шейдера
static GLuint g_format_tex             = 0; ///< Текстура внутри g_format_fbo
static GLuint g_post_program           = 0; ///< Программа пост-обработки
static GLint g_post_uniform_tex        = -1;
static GLint g_post_uniform_saturation = -1;

static uint8_t * g_flip_buf = nullptr;
static size_t g_flip_buf_sz = 0;

#define HIST_BAND_ROWS          (64)
#define HIST_TEX_H              (HIST_BAND_ROWS * 3) // 3 полосы по 64 px для RGB, одна на всю высоту для grayscale
#define HIST_BACKGROUND_OPACITY (20)
#define HIST_BIN_OPACITY        (255)

_Static_assert(HIST_TEX_H % 3 == 0, "HIST_TEX_H must be divisible by 3");


typedef uint8_t hist_pixel_t[ 4 ]; // [R, G, B, A]

static union
{
    hist_pixel_t by_channel[ HIST_CHANNELS ][ HIST_BAND_ROWS ][ HIST_BINS ];
    hist_pixel_t by_row[ HIST_TEX_H ][ HIST_BINS ];
} g_hist_pixels;

// Для переключения fullscreen двойным кликом
static bool g_is_fullscreen     = false;
static int g_windowed_x         = 0;
static int g_windowed_y         = 0;
static int g_windowed_w         = 0;
static int g_windowed_h         = 0;
static double g_last_click_time = 0.0;


static bool g_saturation_enabled = false;

// Для заголовка окна
static char g_format_name[ 64 ] = {0};

typedef struct format_config_s
{
    char name[ 64 ];
    int bytes_per_pixel;
    int gl_internal_format;
    int gl_format;
    int gl_type;
    char shader_file[ 256 ];
    bool needs_demosaic;
    char bayer_pattern[ 8 ];
} format_config_s;

static format_config_s g_format;

static char * trim_inplace(char * s)
{
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
    {
        s++;
    }

    char * end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r'))
    {
        *end-- = '\0';
    }

    return s;
}

static int parse_gl_enum(const char * value)
{
    if (strcmp(value, "GL_RED") == 0)
    {
        return GL_RED;
    }
    if (strcmp(value, "GL_RGB") == 0)
    {
        return GL_RGB;
    }
    if (strcmp(value, "GL_RGBA") == 0)
    {
        return GL_RGBA;
    }
    if (strcmp(value, "GL_R16") == 0)
    {
        return GL_R16;
    }
    if (strcmp(value, "GL_RGB10_A2") == 0)
    {
        return GL_RGB10_A2;
    }
    if (strcmp(value, "GL_UNSIGNED_BYTE") == 0)
    {
        return GL_UNSIGNED_BYTE;
    }
    if (strcmp(value, "GL_UNSIGNED_SHORT") == 0)
    {
        return GL_UNSIGNED_SHORT;
    }
    if (strcmp(value, "GL_UNSIGNED_INT_2_10_10_10_REV") == 0)
    {
        return GL_UNSIGNED_INT_2_10_10_10_REV;
    }
    return -1;
}

static bool parse_format_file(const char * format_name, format_config_s * cfg)
{
    char path[ 256 ];
    snprintf(path, sizeof(path), "formats/%s.fmt", format_name);

    FILE * f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "Cannot open format file: %s\n", path);
        return false;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->bytes_per_pixel    = 1;
    cfg->gl_internal_format = GL_RED;
    cfg->gl_format          = GL_RED;
    cfg->gl_type            = GL_UNSIGNED_BYTE;
    snprintf(cfg->shader_file, sizeof(cfg->shader_file), "%s.glsl", format_name);
    cfg->needs_demosaic = false;
    strcpy(cfg->bayer_pattern, "RGGB");

    char line[ 256 ];
    while (fgets(line, sizeof(line), f))
    {
        char * comment = strchr(line, '#');
        if (comment)
        {
            *comment = '\0';
        }

        char * p = trim_inplace(line);
        if (*p == '\0')
        {
            continue;
        }

        char * eq = strchr(p, '=');
        if (!eq)
        {
            continue;
        }

        *eq          = '\0';
        char * key   = trim_inplace(p);
        char * value = trim_inplace(eq + 1);

        if (strcmp(key, "bytes_per_pixel") == 0)
        {
            long bpp;
            parse_int(value, 1, 128, &bpp);
            cfg->bytes_per_pixel = bpp;
        }
        else if (strcmp(key, "gl_internal_format") == 0)
        {
            int v = parse_gl_enum(value);
            if (v >= 0)
            {
                cfg->gl_internal_format = v;
            }
            else
            {
                fprintf(stderr, "Unknown internal format: %s\n", value);
            }
        }
        else if (strcmp(key, "gl_format") == 0)
        {
            int v = parse_gl_enum(value);
            if (v >= 0)
            {
                cfg->gl_format = v;
            }
            else
            {
                fprintf(stderr, "Unknown format: %s\n", value);
            }
        }
        else if (strcmp(key, "gl_type") == 0)
        {
            int v = parse_gl_enum(value);
            if (v >= 0)
            {
                cfg->gl_type = v;
            }
            else
            {
                fprintf(stderr, "Unknown type: %s\n", value);
            }
        }
        else if (strcmp(key, "shader_file") == 0)
        {
            strncpy(cfg->shader_file, value, sizeof(cfg->shader_file) - 1);
            cfg->shader_file[ sizeof(cfg->shader_file) - 1 ] = '\0';
        }
        else if (strcmp(key, "needs_demosaic") == 0)
        {
            cfg->needs_demosaic = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        }
        else if (strcmp(key, "bayer_pattern") == 0)
        {
            strncpy(cfg->bayer_pattern, value, sizeof(cfg->bayer_pattern) - 1);
        }
    }
    fclose(f);
    return true;
}

// Возвращает указатель на данные кадра, пригодные для glTexSubImage2D.
// OpenGL ожидает строки снизу вверх, а наши буферы — сверху вниз.
// Переворачиваем построчно. Буфер переиспользуется между кадрами.
static const uint8_t * flip_for_upload(const uint8_t * data)
{
    if (!data)
    {
        return nullptr;
    }

    size_t need = (size_t)g_width * (size_t)g_height * (size_t)g_format.bytes_per_pixel;
    if (need == 0)
    {
        return data;
    }

    if (g_flip_buf_sz < need)
    {
        uint8_t * nb = realloc(g_flip_buf, need);
        if (!nb)
        {
            fprintf(stderr, "gl_render: flip buffer alloc failed (%zu bytes)\n", need);
            return data; // отдаём как есть, лучше кривая ориентация, чем ничего
        }
        g_flip_buf    = nb;
        g_flip_buf_sz = need;
    }

    size_t row_bytes = (size_t)g_width * (size_t)g_format.bytes_per_pixel;

    for (int y = 0; y < g_height; y++)
    {
        const uint8_t * src = data + (size_t)y * row_bytes;
        uint8_t * dst       = g_flip_buf + (size_t)(g_height - 1 - y) * row_bytes;
        memcpy(dst, src, row_bytes);
    }

    return g_flip_buf;
}

// Создаёт FBO с RGBA8-текстурой размером W×H. Результат форматного
// шейдера (после демозаики, ycbcr→rgb, gray16-масштаба) кладётся сюда,
// а на экран он попадает уже через пост-шейдер. Пост-шейдер читает
// эту текстуру как обычную sampler2D.
static bool create_format_fbo(void)
{
    glGenTextures(1, &g_format_tex);
    glBindTexture(GL_TEXTURE_2D, g_format_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &g_format_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_format_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_format_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "format FBO incomplete: 0x%x\n", status);
        return false;
    }
    return true;
}

// Создаёт offscreen FBO размером WxH для скриншотов.
// Вызывается лениво при первом запросе. Размеры кадра фиксированы,
// поэтому пересоздавать не нужно.
static bool ensure_shot_fbo(void)
{
    if (g_shot_fbo)
    {
        return true;
    }

    glGenTextures(1, &g_shot_tex);
    glBindTexture(GL_TEXTURE_2D, g_shot_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &g_shot_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_shot_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_shot_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "screenshot FBO incomplete: 0x%x\n", status);
        if (g_shot_tex)
        {
            glDeleteTextures(1, &g_shot_tex);
            g_shot_tex = 0;
        }
        if (g_shot_fbo)
        {
            glDeleteFramebuffers(1, &g_shot_fbo);
            g_shot_fbo = 0;
        }
        return false;
    }
    return true;
}

// Является ли текущий формат YCbCr (Y, Cb, Cr в трёх байтах).
// Пока проверяем по имени формата; если появится второй ycbcr-формат,
// вынести признак в .fmt.
static bool hist_format_is_ycbcr(void)
{
    return g_format_name[ 0 ] != '\0' && strcmp(g_format_name, "ycbcr") == 0;
}

// Рисует CDF-линию поверх гистограммы.
// cdf — массив HIST_BINS значений в [0..1] (нормированный кумулятив).
// x_lo, x_hi — диапазон бинов, попадающий на всю ширину полосы.
// y_top — верхняя строка полосы (куда рисовать CDF).
// band_rows — высота полосы в строках.
// x_map — указатель на массив отображения бина в X-координату (HIST_BINS значений
//         или NULL, если отображение 1:1 — тогда используется x в диапазоне [x_lo..x_hi]).
//
// Здесь мы не используем x_map, а вычисляем xs так же, как build_histogram_image.
static void draw_cdf_line_channel(const float * cdf, int band_index, int x_lo, int x_hi, int span, uint8_t r, uint8_t g,
                                  uint8_t b)
{
    int y0 = band_index * HIST_BAND_ROWS;

    int prev_y = -1;

    for (int x = x_lo; x <= x_hi; x++)
    {
        int xs = (x - x_lo) * HIST_BINS / span;
        if (xs >= HIST_BINS)
        {
            xs = HIST_BINS - 1;
        }

        // CDF = 0 рисуется в самом низу полосы, CDF = 1 — в самом верху.
        int y_local = (int)(cdf[ x ] * (float)(HIST_BAND_ROWS - 1) + 0.5f);
        int y_pix   = y0 + (HIST_BAND_ROWS - 1 - y_local);

        if (prev_y >= 0)
        {
            int ya = prev_y < y_pix ? prev_y : y_pix;
            int yb = prev_y > y_pix ? prev_y : y_pix;
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

static void draw_cdf_line_gray(const float * cdf, uint8_t r, uint8_t g, uint8_t b)
{
    int prev_y = -1;

    for (int x = 0; x < HIST_BINS; x++)
    {
        int y_local = (int)(cdf[ x ] * (float)(HIST_TEX_H - 1) + 0.5f);
        int y_pix   = HIST_TEX_H - 1 - y_local;

        if (prev_y >= 0)
        {
            int ya = prev_y < y_pix ? prev_y : y_pix;
            int yb = prev_y > y_pix ? prev_y : y_pix;
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

// Считает нормированный CDF по бинам одного канала.
// cdf_out — массив HIST_BINS, значения в [0..1].
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

// Заполняет g_hist_pixels растр для текстуры гистограммы.
// Grayscale: одна белая гистограмма на всю высоту.
// RGB: три полосы (R/G/B), полный диапазон 0..255.
// YCbCr: три полосы (Y/Cb/Cr); Y — по всей ширине, Cb/Cr — окно [96..160]
//        вокруг нейтрального 128, растянутое на всю ширину.
// Высота столбика — по логарифмической шкале, с нормировкой на максимум своего канала (иначе один доминирующий пик
// подавит остальные каналы).
// Заполняет g_hist_pixels растр для текстуры гистограммы.
// Grayscale: одна белая гистограмма на всю высоту.
// RGB: три полосы (R/G/B), полный диапазон 0..255.
// YCbCr: три полосы (Y/Cb/Cr); Y — по всей ширине, Cb/Cr — окно [96..160]
//        вокруг нейтрального 128, растянутое на всю ширину.
// Высота столбика — линейная, но нормируется на максимум СВОЕГО канала,
// иначе один доминирующий пик (в G или Y) подавит остальные каналы.
static void build_histogram_image(void)
{
    memset(&g_hist_pixels, 0, sizeof(g_hist_pixels));

    if (g_histogram.active_channels == 1)
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

        // Grayscale: один столбик на всю высоту.
        const uint32_t maxv = g_histogram.max_per_channel[ 0 ];
        if (maxv > 0)
        {
            for (int x = 0; x < HIST_BINS; x++)
            {
                int bar;
                if (g_histogram_log_scale)
                {
                    bar = (int)(log(1.0 + (double)g_histogram.bins[ 0 ][ x ]) / log(1.0 + (double)maxv) * HIST_TEX_H);
                }
                else
                {
                    bar = (int)((uint64_t)g_histogram.bins[ 0 ][ x ] * HIST_TEX_H / maxv);
                }

                for (int y = 0; y < bar; y++)
                {
                    g_hist_pixels.by_row[ HIST_TEX_H - 1 - y ][ x ][ 3 ] = HIST_BIN_OPACITY;
                }
            }
        }

        if (g_histogram_cdf_enabled && maxv > 0)
        {
            float cdf[ HIST_BINS ];
            compute_cdf_channel(g_histogram.bins[ 0 ], cdf);
            draw_cdf_line_gray(cdf, 255, 255, 0);
        }
    }
    else if (g_histogram.active_channels == 3)
    {
        // RGB / YCbCr: три полосы, вью by_channel.
        static const uint8_t col_rgb[ 3 ][ 3 ] = {
            {255, 0,   0  }, // R
            {0,   255, 0  }, // G
            {0,   0,   255}, // B
        };
        static const uint8_t col_ycbcr[ 3 ][ 3 ] = {
            {220, 220, 220}, // Y
            {80,  120, 255}, // Cb
            {255, 80,  80 }, // Cr
        };
        const bool is_ycbcr = hist_format_is_ycbcr();

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

            const uint32_t maxv = g_histogram.max_per_channel[ c ];
            if (maxv == 0)
            {
                continue;
            }

            for (int x = lo; x <= hi; x++)
            {
                int bar;
                if (g_histogram_log_scale)
                {
                    bar =
                        (int)(log(1.0 + (double)g_histogram.bins[ c ][ x ]) / log(1.0 + (double)maxv) * HIST_BAND_ROWS);
                }
                else
                {
                    bar = (int)((uint64_t)g_histogram.bins[ c ][ x ] * HIST_BAND_ROWS / maxv);
                }

                int xs = (x - lo) * HIST_BINS / span;

                for (int y = 0; y < bar; y++)
                {
                    g_hist_pixels.by_channel[ c ][ HIST_BAND_ROWS - 1 - y ][ xs ][ 3 ] = HIST_BIN_OPACITY;
                }
            }

            if (g_histogram_cdf_enabled)
            {
                static const uint8_t cdf_colors[ 3 ][ 3 ] = {
                    {255, 255, 0  }, // жёлтый
                    {0,   255, 255}, // cyan
                    {255, 0,   255}, // magenta
                };

                for (int c = 0; c < 3; c++)
                {
                    float cdf[ HIST_BINS ];
                    compute_cdf_channel(g_histogram.bins[ c ], cdf);

                    const bool use_window = is_ycbcr && c != 0;
                    const int lo          = use_window ? 96 : 0;
                    const int hi          = use_window ? 160 : (HIST_BINS - 1);
                    const int span        = hi - lo + 1;

                    draw_cdf_line_channel(cdf, c, lo, hi, span, cdf_colors[ c ][ 0 ], cdf_colors[ c ][ 1 ],
                                          cdf_colors[ c ][ 2 ]);
                }
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D, g_hist_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HIST_BINS, HIST_TEX_H, GL_RGBA, GL_UNSIGNED_BYTE, &g_hist_pixels);
}

// Считаем гистограмму по сырому буферу кадра.
// Поддерживаемые форматы:
//   GL_RED + UNSIGNED_BYTE  → 1 канал, 8 бит
//   GL_RED + UNSIGNED_SHORT → 1 канал, берём старший байт
//   GL_RGB/GL_RGBA + UNSIGNED_BYTE → 3 канала
// Прочее → активных каналов 0 (ничего не рисуем).
static void compute_histogram(const uint8_t * data)
{
    histogram_reset(&g_histogram);

    const size_t n = (size_t)g_width * (size_t)g_height;

    if (g_format.gl_format == GL_RED && g_format.gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 1;
        for (size_t i = 0; i < n; i++)
        {
            histogram_add(&g_histogram, 0, data[ i ]);
        }
    }
    else if (g_format.gl_format == GL_RED && g_format.gl_type == GL_UNSIGNED_SHORT)
    {
        g_histogram.active_channels = 1;
        const uint16_t * p          = (const uint16_t *)data;
        for (size_t i = 0; i < n; i++)
        {
            histogram_add(&g_histogram, 0, p[ i ] >> 8);
        }
    }
    else if ((g_format.gl_format == GL_RGB || g_format.gl_format == GL_RGBA) && g_format.gl_type == GL_UNSIGNED_BYTE)
    {
        g_histogram.active_channels = 3;
        const int step              = (g_format.gl_format == GL_RGBA) ? 4 : 3;
        for (size_t i = 0; i < n; i++)
        {
            const uint8_t * px = data + (i * (size_t)step);
            histogram_add(&g_histogram, 0, px[ 0 ]);
            histogram_add(&g_histogram, 1, px[ 1 ]);
            histogram_add(&g_histogram, 2, px[ 2 ]);
        }
    }
    else
    {
        g_histogram.active_channels = 0;
    }

    histogram_finalize(&g_histogram);
}

// Сохранить текущий кадр в PNG в оригинальном размере (W×H).
// Рендерим ещё раз в offscreen FBO размером с кадр, читаем пиксели оттуда,
// пишем PNG. Default framebuffer при этом не трогается.
static void save_screenshot_internal(void)
{
    if (!ensure_shot_fbo())
    {
        return;
    }

    // Рендер кадра в FBO (без letterbox — ровно W×H).
    glBindFramebuffer(GL_FRAMEBUFFER, g_shot_fbo);
    glViewport(0, 0, g_width, g_height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    if (g_uniform_tex >= 0)
    {
        glUniform1i(g_uniform_tex, 0);
    }
    if (g_uniform_width >= 0)
    {
        glUniform1i(g_uniform_width, g_width);
    }
    if (g_uniform_height >= 0)
    {
        glUniform1i(g_uniform_height, g_height);
    }
    if (g_uniform_effective_bits >= 0)
    {
        glUniform1i(g_uniform_effective_bits, g_eff_bits);
    }
    if (g_uniform_demosaic_mode >= 0)
    {
        glUniform1i(g_uniform_demosaic_mode, g_demosaic_mode);
    }

    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    size_t stride    = (size_t)g_width * 4;
    uint8_t * pixels = malloc(stride * (size_t)g_height);
    if (!pixels)
    {
        fprintf(stderr, "screenshot: out of memory\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // OpenGL отдаёт строки снизу вверх — переворачиваем.
    for (int y = 0; y < g_height / 2; y++)
    {
        uint8_t * a = pixels + ((size_t)y * stride);
        uint8_t * b = pixels + ((size_t)(g_height - 1 - y) * stride);
        for (size_t i = 0; i < stride; i++)
        {
            uint8_t t = a[ i ];
            a[ i ]    = b[ i ];
            b[ i ]    = t;
        }
    }

    time_t t           = time(nullptr);
    struct tm * tm_now = localtime(&t);

    char stamp[ 32 ];
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", tm_now);

    const char * outdir = saver_get_outdir();
    char path[ 1024 ];
    snprintf(path, sizeof(path), "%s/screenshot_%s.png", outdir, stamp);

    if (png_write_rgba(path, pixels, g_width, g_height))
    {
        printf("Screenshot saved: %s (%dx%d)\n", path, g_width, g_height);
    }
    else
    {
        fprintf(stderr, "Failed to save screenshot: %s\n", path);
    }

    free(pixels);
}

static GLuint compile_shader(const char * source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info[ 512 ];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        fprintf(stderr, "Shader compilation error: %s\n", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint build_program(const char * fragment_source)
{
    static const char * vertex_source =
        "#version 330 core\n"
        "layout(location=0) in vec2 aPos;\n"
        "layout(location=1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "}\n";

    GLuint vertex = compile_shader(vertex_source, GL_VERTEX_SHADER);
    if (!vertex)
    {
        return 0;
    }

    GLuint fragment = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    if (!fragment)
    {
        glDeleteShader(vertex);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertex);
    glAttachShader(prog, fragment);
    glLinkProgram(prog);

    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info[ 512 ];
        glGetProgramInfoLog(prog, sizeof(info), nullptr, info);
        fprintf(stderr, "Program linking error: %s\n", info);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(prog);
        return 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return prog;
}

static void create_fullscreen_quad(GLuint * out_vao)
{
    static const float vertices[] = {
        -1.0f, 1.0f,  0.0f, 0.0f, //
        -1.0f, -1.0f, 0.0f, 1.0f, //
        1.0f,  -1.0f, 1.0f, 1.0f, //
        -1.0f, 1.0f,  0.0f, 0.0f, //
        1.0f,  -1.0f, 1.0f, 1.0f, //
        1.0f,  1.0f,  1.0f, 0.0f, //
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    *out_vao = vao;
}

// Формируем заголовок: "RTP Viewer - WxH format [SMOOTH] [REC]"
void gl_render_refresh_title(void)
{
    if (!g_window)
    {
        return;
    }

    char title[ 160 ];

    snprintf(title, sizeof(title), "RTP Viewer - %dx%d %s fps:%.1f%s%s%s", //
             g_width, g_height,                                            //
             g_format_name[ 0 ] ? g_format_name : "?",                     //
             stats_get_fps(),                                              //
             g_smoothing ? " [SMOOTH]" : "",                               //
             g_paused ? " [PAUSE]" : "",                                   //
             saver_is_recording() ? " [REC]" : "");

    glfwSetWindowTitle(g_window, title);
}

static void mouse_button_callback(GLFWwindow * /*unused*/, int button, int action, int /*unused*/)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
    {
        return;
    }

    double now = glfwGetTime();

    // Двойной клик: второй клик в течение 400 мс после первого
    if (now - g_last_click_time < 0.4)
    {
        gl_render_toggle_fullscreen();
        g_last_click_time = 0.0; // сброс, чтобы тройной клик не сработал дважды
    }
    else
    {
        g_last_click_time = now;
    }
}

void gl_render_toggle_fullscreen(void)
{
    if (!g_window)
    {
        return;
    }

    if (!g_is_fullscreen)
    {
        // Сохраняем текущую позицию и размер окна
        glfwGetWindowPos(g_window, &g_windowed_x, &g_windowed_y);
        glfwGetWindowSize(g_window, &g_windowed_w, &g_windowed_h);

        // Находим монитор, на котором сейчас находится окно
        GLFWmonitor * target = nullptr;

        int win_center_x = g_windowed_x + (g_windowed_w / 2);
        int win_center_y = g_windowed_y + (g_windowed_h / 2);

        int monitor_count       = 0;
        GLFWmonitor ** monitors = glfwGetMonitors(&monitor_count);

        for (int i = 0; i < monitor_count; i++)
        {
            int mx = 0, my = 0;
            glfwGetMonitorPos(monitors[ i ], &mx, &my);

            const GLFWvidmode * mode = glfwGetVideoMode(monitors[ i ]);
            if (!mode)
            {
                continue;
            }

            // Границы монитора в виртуальных координатах
            int m_left   = mx;
            int m_top    = my;
            int m_right  = mx + mode->width;
            int m_bottom = my + mode->height;

            if (win_center_x >= m_left && win_center_x < m_right && win_center_y >= m_top && win_center_y < m_bottom)
            {
                target = monitors[ i ];
                break;
            }
        }

        // Если по каким-то причинам не нашли — берём основной
        if (!target)
        {
            target = glfwGetPrimaryMonitor();
        }

        if (!target)
        {
            return;
        }

        const GLFWvidmode * mode = glfwGetVideoMode(target);
        if (!mode)
        {
            return;
        }

        glfwSetWindowMonitor(g_window, target, 0, 0, mode->width, mode->height, mode->refreshRate);

        g_is_fullscreen = true;
    }
    else
    {
        // Возврат в оконный режим
        glfwSetWindowMonitor(g_window, nullptr, g_windowed_x, g_windowed_y, g_windowed_w, g_windowed_h, 0);

        g_is_fullscreen = false;
    }
}

bool gl_render_init(int width, int height, const char * format_name, int effective_bits, bool smooth_start)
{
    if (!glfwInit())
    {
        fprintf(stderr, "GLFW init failed\n");
        return false;
    }

    if (!parse_format_file(format_name, &g_format))
    {
        glfwTerminate();
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    g_window = glfwCreateWindow(width, height, "RTP Viewer", nullptr, nullptr);
    if (!g_window)
    {
        fprintf(stderr, "GLFW window creation failed\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    glfwSetMouseButtonCallback(g_window, mouse_button_callback);

    if (gl_api_load() != 0)
    {
        fprintf(stderr, "Failed to load OpenGL functions\n");
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    text_overlay_init();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // ---- Размеры и имя формата — до всего, что их использует ----
    g_width    = width;
    g_height   = height;
    g_eff_bits = effective_bits;
    snprintf(g_format_name, sizeof(g_format_name), "%s", format_name);

    // ---- Форматный шейдер ----
    char shader_path[ 512 ];
    snprintf(shader_path, sizeof(shader_path), "formats/%s", g_format.shader_file);

    char * frag_source = read_file(shader_path);
    if (!frag_source)
    {
        fprintf(stderr, "Failed to load shader file: %s\n", shader_path);
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    g_program = build_program(frag_source);
    free(frag_source);
    if (!g_program)
    {
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    glUseProgram(g_program);
    g_uniform_tex            = glGetUniformLocation(g_program, "tex");
    g_uniform_width          = glGetUniformLocation(g_program, "image_width");
    g_uniform_height         = glGetUniformLocation(g_program, "image_height");
    g_uniform_effective_bits = glGetUniformLocation(g_program, "effective_bits");
    g_uniform_demosaic_mode  = glGetUniformLocation(g_program, "demosaic_mode");

    if (g_uniform_width >= 0)
    {
        glUniform1i(g_uniform_width, width);
    }
    if (g_uniform_height >= 0)
    {
        glUniform1i(g_uniform_height, height);
    }
    if (g_uniform_effective_bits >= 0)
    {
        glUniform1i(g_uniform_effective_bits, effective_bits);
    }
    if (g_uniform_demosaic_mode >= 0)
    {
        glUniform1i(g_uniform_demosaic_mode, 0);
    }

    // ---- Сырая текстура ----
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, g_format.gl_internal_format, width, height, 0, g_format.gl_format, g_format.gl_type,
                 nullptr);

    // ---- FBO форматного прохода (g_width/g_height уже заданы) ----
    if (!create_format_fbo())
    {
        glDeleteProgram(g_program);
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    // ---- Пост-шейдер ----
    char * post_src = read_file("formats/post.glsl");
    if (!post_src)
    {
        fprintf(stderr, "Failed to load formats/post.glsl\n");
        glDeleteProgram(g_program);
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }
    g_post_program = build_program(post_src);
    free(post_src);
    if (!g_post_program)
    {
        glDeleteProgram(g_program);
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    glUseProgram(g_post_program);
    g_post_uniform_tex        = glGetUniformLocation(g_post_program, "tex");
    g_post_uniform_saturation = glGetUniformLocation(g_post_program, "saturation_enabled");

    // ---- Сглаживание — после создания g_format_tex ----
    g_smoothing = !smooth_start;
    gl_render_toggle_smoothing();

    // ---- Оверлей гистограммы ----
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

    g_hist_program = build_program(hist_frag);
    if (!g_hist_program)
    {
        glDeleteProgram(g_program);
        glDeleteProgram(g_post_program);
        glDeleteTextures(1, &g_texture);
        glDeleteTextures(1, &g_format_tex);
        glDeleteFramebuffers(1, &g_format_fbo);
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }
    glUseProgram(g_hist_program);
    g_hist_uniform_tex = glGetUniformLocation(g_hist_program, "tex");

    // Blending — для оверлея. Основной шейдер выдаёт alpha=1, на него не влияет.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    create_fullscreen_quad(&g_vao);

    return true;
}
GLFWwindow * gl_render_get_window(void)
{
    return g_window;
}

void gl_render_update_texture(const uint8_t * data)
{
    // Загрузка сырого кадра. OpenGL ожидает bottom-up, у нас top-down —
    // переворачиваем через отдельный буфер, чтобы не портить frame_buffer
    // (он нужен saver’у и гистограмме в исходной ориентации).
    const uint8_t * upload = flip_for_upload(data);

    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_width, g_height, g_format.gl_format, g_format.gl_type, upload);

    if (g_histogram_enabled)
    {
        compute_histogram(data);
        build_histogram_image();
    }

    int win_w = 0;
    int win_h = 0;
    glfwGetFramebufferSize(g_window, &win_w, &win_h);

    if (win_w <= 0 || win_h <= 0)
    {
        return;
    }

    // --- Pass 1: форматный шейдер → g_format_fbo ---
    glBindFramebuffer(GL_FRAMEBUFFER, g_format_fbo);
    glViewport(0, 0, g_width, g_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    if (g_uniform_tex >= 0)
    {
        glUniform1i(g_uniform_tex, 0);
    }
    if (g_uniform_width >= 0)
    {
        glUniform1i(g_uniform_width, g_width);
    }
    if (g_uniform_height >= 0)
    {
        glUniform1i(g_uniform_height, g_height);
    }
    if (g_uniform_effective_bits >= 0)
    {
        glUniform1i(g_uniform_effective_bits, g_eff_bits);
    }
    if (g_uniform_demosaic_mode >= 0)
    {
        glUniform1i(g_uniform_demosaic_mode, g_demosaic_mode);
    }

    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- Pass 2: пост-шейдер → default FB ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    float aspect_img = (float)g_width / (float)g_height;
    float aspect_win = (float)win_w / (float)win_h;

    int vp_w = 0, vp_h = 0, vp_x = 0, vp_y = 0;
    if (aspect_win > aspect_img)
    {
        vp_h = win_h;
        vp_w = (int)(win_h * aspect_img);
        vp_x = (win_w - vp_w) / 2;
    }
    else
    {
        vp_w = win_w;
        vp_h = (int)(win_w / aspect_img);
        vp_y = (win_h - vp_h) / 2;
    }

    glViewport(vp_x, vp_y, vp_w, vp_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_post_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_format_tex);
    if (g_post_uniform_tex >= 0)
    {
        glUniform1i(g_post_uniform_tex, 0);
    }
    if (g_post_uniform_saturation >= 0)
    {
        glUniform1i(g_post_uniform_saturation, g_saturation_enabled ? 1 : 0);
    }

    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- Оверлей гистограммы ---
    if (g_histogram_enabled && g_histogram.active_channels > 0)
    {
        const int ov_w   = 320;
        const int ov_h   = 240;
        const int margin = 10;

        int ov_x = win_w - ov_w - margin;
        int ov_y = win_h - ov_h - margin;
        if (ov_x < 0)
        {
            ov_x = 0;
        }
        if (ov_y < 0)
        {
            ov_y = 0;
        }

        glViewport(ov_x, ov_y, ov_w, ov_h);

        glUseProgram(g_hist_program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_hist_tex);
        if (g_hist_uniform_tex >= 0)
        {
            glUniform1i(g_hist_uniform_tex, 0);
        }

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);

    // --- Текстовый оверлей ---
    text_begin(win_w, win_h);
    // ... здесь позже появятся text_draw(...)
    text_end();

    // --- Скриншот (чистый кадр, без пост-обработки и оверлеев) ---
    if (g_screenshot_requested)
    {
        g_screenshot_requested = false;
        save_screenshot_internal();
    }

    glfwSwapBuffers(g_window);
}

void gl_render_poll_events(void)
{
    glfwPollEvents();
}

void gl_render_cleanup(void)
{
    if (g_vao)
    {
        glDeleteVertexArrays(1, &g_vao);
    }
    if (g_program)
    {
        glDeleteProgram(g_program);
    }
    if (g_shot_tex)
    {
        glDeleteTextures(1, &g_shot_tex);
    }
    if (g_shot_fbo)
    {
        glDeleteFramebuffers(1, &g_shot_fbo);
    }
    if (g_texture)
    {
        glDeleteTextures(1, &g_texture);
    }
    if (g_window)
    {
        glfwDestroyWindow(g_window);
    }
    if (g_hist_program)
    {
        glDeleteProgram(g_hist_program);
    }
    if (g_hist_tex)
    {
        glDeleteTextures(1, &g_hist_tex);
    }
    if (g_post_program)
    {
        glDeleteProgram(g_post_program);
    }
    if (g_format_tex)
    {
        glDeleteTextures(1, &g_format_tex);
    }
    if (g_format_fbo)
    {
        glDeleteFramebuffers(1, &g_format_fbo);
    }
    text_overlay_cleanup();
    glfwTerminate();

    free(g_flip_buf);

    g_flip_buf     = nullptr;
    g_flip_buf_sz  = 0;
    g_vao          = 0;
    g_shot_tex     = 0;
    g_shot_fbo     = 0;
    g_program      = 0;
    g_texture      = 0;
    g_hist_program = 0;
    g_hist_tex     = 0;
    g_window       = nullptr;
}

void gl_render_toggle_smoothing(void)
{
    g_smoothing = !g_smoothing;

    // Сырая текстура всегда NEAREST — форматный шейдер берёт точные тексели
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Сглаживание применяется к результату форматного шейдера
    glBindTexture(GL_TEXTURE_2D, g_format_tex);
    if (g_smoothing)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    gl_render_refresh_title();
    printf("Smoothing %s\n", g_smoothing ? "enabled" : "disabled");
}

void gl_render_set_demosaic_mode(int mode)
{
    g_demosaic_mode = mode;
}

int gl_render_get_bytes_per_pixel(void)
{
    return g_format.bytes_per_pixel;
}

void gl_render_request_screenshot(void)
{
    g_screenshot_requested = true;
}

void gl_render_toggle_histogram(void)
{
    g_histogram_enabled = !g_histogram_enabled;
    gl_render_refresh_title();
    printf("Histogram %s\n", g_histogram_enabled ? "enabled" : "disabled");
}

void gl_render_toggle_histogram_scale(void)
{
    g_histogram_log_scale = !g_histogram_log_scale;
    printf("Histogram scale: %s\n", g_histogram_log_scale ? "log" : "linear");
    // Перерисовывать немедленно не нужно: build_histogram_image вызывается
    // на следующем кадре в update_texture. Если хочется увидеть сразу —
    // потребуется доступ к последнему сырому буферу, что усложнит API.
}

void gl_render_toggle_histogram_cdf(void)
{
    g_histogram_cdf_enabled = !g_histogram_cdf_enabled;
    printf("Histogram CDF %s\n", g_histogram_cdf_enabled ? "enabled" : "disabled");
}

void gl_render_toggle_saturation(void)
{
    g_saturation_enabled = !g_saturation_enabled;
    gl_render_refresh_title();
    printf("Saturation markers %s\n", g_saturation_enabled ? "enabled" : "disabled");
}

void gl_render_set_paused(bool paused)
{
    g_paused = paused;
    gl_render_refresh_title();
}
