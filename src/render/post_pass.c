#include "post_pass.h"

#include <stddef.h>
#include <stdio.h>

#include "gl_utils.h"

static char POST_SHADER_FRAGMENT[] = {
#embed "../res/post.glsl" suffix(, 0)
};

static GLuint g_program  = 0;
static GLuint g_quad_vao = 0;

static bool g_gradient_enabled    = false;
static bool g_auto_levels_enabled = false;

static GLint g_uniform_tex         = -1;
static GLint g_uniform_saturation  = -1;
static GLint g_uniform_gradient    = -1;
static GLint g_uniform_texel_size  = -1;
static GLint g_uniform_origin      = -1;
static GLint g_uniform_size        = -1;
static GLint g_uniform_auto_levels = -1;
static GLint g_uniform_level_min   = -1;
static GLint g_uniform_level_max   = -1;

static float g_level_min[ 3 ] = {0.0f, 0.0f, 0.0f};
static float g_level_max[ 3 ] = {1.0f, 1.0f, 1.0f};

static fimage_size_s g_texel = {};

static bool g_saturation_enabled = false;

bool post_pass_init(GLuint quad_vao)
{
    g_quad_vao = quad_vao;

    g_program = gl_utils_build_program(POST_SHADER_FRAGMENT);
    if (!g_program)
    {
        return false;
    }

    glUseProgram(g_program);
    g_uniform_tex         = glGetUniformLocation(g_program, "tex");
    g_uniform_saturation  = glGetUniformLocation(g_program, "saturation_enabled");
    g_uniform_gradient    = glGetUniformLocation(g_program, "gradient_enabled");
    g_uniform_texel_size  = glGetUniformLocation(g_program, "texel_size");
    g_uniform_origin      = glGetUniformLocation(g_program, "image_origin");
    g_uniform_size        = glGetUniformLocation(g_program, "image_size");
    g_uniform_auto_levels = glGetUniformLocation(g_program, "auto_levels_enabled");
    g_uniform_level_min   = glGetUniformLocation(g_program, "level_min");
    g_uniform_level_max   = glGetUniformLocation(g_program, "level_max");

    return true;
}

void post_pass_cleanup(void)
{
    if (g_program)
    {
        glDeleteProgram(g_program);
        g_program = 0;
    }
}

void post_pass_render(GLuint source_tex, point_s dst, image_size_s size, image_size_s win)
{
    if (win.x <= 0 || win.y <= 0 || size.x <= 0 || size.y <= 0)
    {
        return;
    }

    // dst — top-left в координатах окна. Для шейдера нужен bottom-left (gl).
    int gl_y = win.y - dst.y - size.y;

    // Всегда полный экран. Никаких огромных вьюпортов — иначе GL_MAX_VIEWPORT_DIMS
    // молча обрежет его при zoom >= 20 (AMD) или 40 (NVIDIA), и картинка «прыгнет».
    glViewport(0, 0, win.x, win.y);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source_tex);
    if (g_uniform_tex >= 0)
    {
        glUniform1i(g_uniform_tex, 0);
    }
    if (g_uniform_saturation >= 0)
    {
        glUniform1i(g_uniform_saturation, g_saturation_enabled ? 1 : 0);
    }
    if (g_uniform_gradient >= 0)
    {
        glUniform1i(g_uniform_gradient, g_gradient_enabled ? 1 : 0);
    }
    if (g_uniform_texel_size >= 0)
    {
        glUniform2f(g_uniform_texel_size, g_texel.x, g_texel.y);
    }
    if (g_uniform_origin >= 0)
    {
        glUniform2f(g_uniform_origin, (float)dst.x, (float)gl_y);
    }
    if (g_uniform_size >= 0)
    {
        glUniform2f(g_uniform_size, (float)size.x, (float)size.y);
    }
    if (g_uniform_auto_levels >= 0)
    {
        glUniform1i(g_uniform_auto_levels, g_auto_levels_enabled ? 1 : 0);
    }
    if (g_uniform_level_min >= 0)
    {
        glUniform3f(g_uniform_level_min, g_level_min[ 0 ], g_level_min[ 1 ], g_level_min[ 2 ]);
    }
    if (g_uniform_level_max >= 0)
    {
        glUniform3f(g_uniform_level_max, g_level_max[ 0 ], g_level_max[ 1 ], g_level_max[ 2 ]);
    }

    glBindVertexArray(g_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void post_pass_set_saturation(bool enabled)
{
    g_saturation_enabled = enabled;
}

void post_pass_set_gradient(bool enabled)
{
    g_gradient_enabled = enabled;
}

void post_pass_set_texel_size(fimage_size_s size)
{
    g_texel = size;
}

void post_pass_set_auto_levels(bool enabled)
{
    g_auto_levels_enabled = enabled;
}

void post_pass_set_levels(const float minv[ static 3 ], const float maxv[ static 3 ])
{
    for (int c = 0; c < 3; c++)
    {
        g_level_min[ c ] = minv[ c ];
        g_level_max[ c ] = maxv[ c ];
    }
}
