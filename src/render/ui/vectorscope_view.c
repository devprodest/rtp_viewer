#include "vectorscope_view.h"

#include "analysis/analytics.h"
#include "app/settings.h"
#include "log/logger.h"
#include "render/gl/gl_utils.h"
#include "render/ui/panel.h"
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

static point_s g_pos = {-1, -1};
static uint8_t g_pixels[ VS_BINS ][ VS_BINS ][ 4 ];

static bool g_enabled  = false;
static bool g_has_data = false;


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

void vectorscope_view_update(void)
{
    const analytics_vectorscope_s * vs = analytics_get_vectorscope();

    g_has_data = vs->valid;
    if (!g_has_data)
    {
        return;
    }

    // --- Построение текстуры ---
    memset(g_pixels, 0, sizeof(g_pixels));

    // Фон
    for (int y = 0; y < VS_BINS; y++)
    {
        for (int x = 0; x < VS_BINS; x++)
        {
            g_pixels[ y ][ x ][ 0 ] = 20;
            g_pixels[ y ][ x ][ 1 ] = 20;
            g_pixels[ y ][ x ][ 2 ] = 20;
            g_pixels[ y ][ x ][ 3 ] = 200;
        }
    }

    const float reference = utils_max((float)vs->max_count / 10.0f, 100.0f);
    const float log_max   = logf(1.0f + reference);
    for (int y = 0; y < VS_BINS; y++)
    {
        for (int x = 0; x < VS_BINS; x++)
        {
            if (vs->bins[ y ][ x ] == 0)
            {
                continue;
            }
            const float t   = logf(1.0f + (float)vs->bins[ y ][ x ]) / log_max;
            const uint8_t v = (uint8_t)(t * 255.0f);

            g_pixels[ y ][ x ][ 0 ] = v;
            g_pixels[ y ][ x ][ 1 ] = v;
            g_pixels[ y ][ x ][ 2 ] = v;
        }
    }

    // Оси
    for (int i = 0; i < VS_BINS; i++)
    {
        g_pixels[ 128 ][ i ][ 0 ] = 80;
        g_pixels[ 128 ][ i ][ 1 ] = 80;
        g_pixels[ 128 ][ i ][ 2 ] = 80;
        g_pixels[ i ][ 128 ][ 0 ] = 80;
        g_pixels[ i ][ 128 ][ 1 ] = 80;
        g_pixels[ i ][ 128 ][ 2 ] = 80;
    }

    // Маркеры hue
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

void vectorscope_view_render(point_s pos, image_size_s win, float dpi_scale)
{
    if (!g_enabled || !g_has_data || !g_program)
    {
        return;
    }

    const image_size_s view = vectorscope_view_get_size();

    if (pos.x < 0 || pos.y < 0)
    {
        const int mx = (int)(10 * dpi_scale);
        const int my = (int)(40 * dpi_scale);
        pos          = (point_s) {utils_max(win.x - view.x - mx, 0),
                                  utils_max(win.y - view.y - my - panel_title_height(dpi_scale), 0)};
        g_pos        = pos;
    }

    int title_h   = panel_title_height(dpi_scale);
    int content_y = pos.y + title_h;
    int gl_y      = win.y - content_y - view.y;

    glViewport(pos.x, gl_y, view.x, view.y);
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
    settings_set_panel_enabled("vectorscope", g_enabled);
    log_info("Vectorscope %s", g_enabled ? "enabled" : "disabled");
}

bool vectorscope_view_is_enabled(void)
{
    return g_enabled;
}

void vectorscope_view_disable(void)
{
    if (!g_enabled)
    {
        return;
    }
    g_enabled = false;
    settings_set_panel_enabled("vectorscope", false);
    log_info("Vectorscope disabled");
}

point_s vectorscope_view_get_pos(void)
{
    return g_pos;
}

void vectorscope_view_set_pos(point_s pos)
{
    g_pos = pos;
}

image_size_s vectorscope_view_get_size(void)
{
    return (image_size_s) {VS_VIEW, VS_VIEW};
}
