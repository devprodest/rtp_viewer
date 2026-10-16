#include "post_pass.h"

#include <stddef.h>
#include <stdio.h>

#include "gl_utils.h"

static char POST_SHADER_FRAGMENT[] = {
#embed "../res/post.glsl"
    ,
    0,
};

static GLuint g_program           = 0;
static GLuint g_quad_vao          = 0;
static GLint g_uniform_tex        = -1;
static GLint g_uniform_saturation = -1;

static GLint g_uniform_gradient   = -1;
static GLint g_uniform_texel_size = -1;
static bool g_gradient_enabled    = false;
static float g_texel_x            = 0.0f;
static float g_texel_y            = 0.0f;

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
    g_uniform_tex        = glGetUniformLocation(g_program, "tex");
    g_uniform_saturation = glGetUniformLocation(g_program, "saturation_enabled");
    g_uniform_gradient   = glGetUniformLocation(g_program, "gradient_enabled");
    g_uniform_texel_size = glGetUniformLocation(g_program, "texel_size");

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

void post_pass_render(GLuint source_tex, int dst_x, int dst_y, int dst_w, int dst_h)
{
    glViewport(dst_x, dst_y, dst_w, dst_h);
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

    glBindVertexArray(g_quad_vao);
    if (g_uniform_gradient >= 0)
    {
        glUniform1i(g_uniform_gradient, g_gradient_enabled ? 1 : 0);
    }
    if (g_uniform_texel_size >= 0)
    {
        glUniform2f(g_uniform_texel_size, g_texel_x, g_texel_y);
    }
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

void post_pass_set_texel_size(float texel_x, float texel_y)
{
    g_texel_x = texel_x;
    g_texel_y = texel_y;
}
