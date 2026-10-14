#include "post_pass.h"

#include <stddef.h>
#include <stdio.h>

#include "gl_utils.h"

/// \brief Исходник пост-шейдера, вкомпилированный в бинарник.
///
/// \details Пост-обработка — часть приложения, не пользовательская
///          конфигурация. Пользователь правит formats/*.glsl + .fmt.
static char POST_SHADER_FRAGMENT[] = {
#embed "../res/post.glsl"
    ,
    0,
};

static GLuint g_program           = 0;
static GLuint g_quad_vao          = 0;
static GLint g_uniform_tex        = -1;
static GLint g_uniform_saturation = -1;

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
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void post_pass_set_saturation(bool enabled)
{
    g_saturation_enabled = enabled;
}
