#include "flicker_view.h"

#include "analysis/flicker.h"
#include "log/logger.h"
#include "render/gl/gl_utils.h"
#include "render/ui/panel.h"
#include "util/utils.h"

#include <string.h>

#define FLICKER_TEX_W 320
#define FLICKER_TEX_H 160

static GLuint g_quad_vao   = 0;
static GLuint g_tex        = 0;
static GLuint g_program    = 0;
static GLint g_uniform_tex = -1;

static point_s g_pos  = {-1, -1};
static bool g_enabled = false;
static uint8_t g_pixels[ FLICKER_TEX_H ][ FLICKER_TEX_W ][ 4 ];

//------------------------------------------------------- API ---------------------------------------------------------

bool flicker_view_init(GLuint quad_vao)
{
    g_quad_vao = quad_vao;

    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FLICKER_TEX_W, FLICKER_TEX_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

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

void flicker_view_cleanup(void)
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

void flicker_view_update(void)
{
    if (!g_enabled)
    {
        return;
    }
    flicker_render_rgba((uint8_t *)g_pixels, FLICKER_TEX_W, FLICKER_TEX_H);

    glBindTexture(GL_TEXTURE_2D, g_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FLICKER_TEX_W, FLICKER_TEX_H, GL_RGBA, GL_UNSIGNED_BYTE, g_pixels);
}

void flicker_view_render(point_s pos, image_size_s win, float dpi_scale)
{
    if (!g_enabled || !g_program)
    {
        return;
    }

    const image_size_s view = flicker_view_get_size();

    if (pos.x < 0 || pos.y < 0)
    {
        const int mx = (int)(10 * dpi_scale);
        const int my = (int)(40 * dpi_scale);
        pos          = (point_s) {mx, my};
        g_pos        = pos;
    }

    const int title_h   = panel_title_height(dpi_scale);
    const int content_y = pos.y + title_h;
    const int gl_y      = win.y - content_y - view.y;

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

void flicker_view_toggle_enabled(void)
{
    g_enabled = !g_enabled;
    log_info("Flicker %s", g_enabled ? "enabled" : "disabled");
}

bool flicker_view_is_enabled(void)
{
    return g_enabled;
}

void flicker_view_disable(void)
{
    if (!g_enabled)
    {
        return;
    }
    g_enabled = false;
    log_info("Flicker disabled");
}

point_s flicker_view_get_pos(void)
{
    return g_pos;
}

void flicker_view_set_pos(point_s pos)
{
    g_pos = pos;
}

image_size_s flicker_view_get_size(void)
{
    return (image_size_s) {FLICKER_TEX_W, FLICKER_TEX_H};
}
