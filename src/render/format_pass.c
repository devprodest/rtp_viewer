#include "format_pass.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gl_api.h"
#include "gl_utils.h"
#include "util/utils.h"

static int g_width    = 0;
static int g_height   = 0;
static int g_eff_bits = 16;

static format_config_s g_format;

static GLuint g_texture    = 0;
static GLuint g_program    = 0;
static GLuint g_format_fbo = 0;
static GLuint g_format_tex = 0;
static GLuint g_quad_vao   = 0;

static GLint g_uniform_tex            = -1;
static GLint g_uniform_width          = -1;
static GLint g_uniform_height         = -1;
static GLint g_uniform_effective_bits = -1;
static GLint g_uniform_demosaic_mode  = -1;

static int g_demosaic_mode = 0;

static uint8_t * g_flip_buf = nullptr;
static size_t g_flip_buf_sz = 0;

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
            fprintf(stderr, "format_pass: flip buffer alloc failed (%zu bytes)\n", need);
            return data;
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

bool format_pass_init(int width, int height, GLuint quad_vao, int effective_bits, const format_config_s * cfg)
{
    g_width    = width;
    g_height   = height;
    g_eff_bits = effective_bits;
    g_format   = *cfg;
    g_quad_vao = quad_vao;

    if (g_quad_vao == 0)
    {
        fprintf(stderr, "format_pass: quad VAO is 0\n");
        return false;
    }

    char shader_path[ 512 ];
    snprintf(shader_path, sizeof(shader_path), "formats/%s", g_format.shader_file);

    char * frag_source = read_file(shader_path);
    if (!frag_source)
    {
        fprintf(stderr, "Failed to load shader file: %s\n", shader_path);
        return false;
    }

    g_program = gl_utils_build_program(frag_source);
    free(frag_source);
    if (!g_program)
    {
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
        glUniform1i(g_uniform_effective_bits, g_eff_bits);
    }
    if (g_uniform_demosaic_mode >= 0)
    {
        glUniform1i(g_uniform_demosaic_mode, 0);
    }

    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, g_format.gl_internal_format, width, height, 0, g_format.gl_format, g_format.gl_type,
                 nullptr);

    if (!create_format_fbo())
    {
        glDeleteProgram(g_program);
        g_program = 0;
        return false;
    }
    return true;
}

void format_pass_cleanup(void)
{
    if (g_program)
    {
        glDeleteProgram(g_program);
    }
    if (g_texture)
    {
        glDeleteTextures(1, &g_texture);
    }
    if (g_format_tex)
    {
        glDeleteTextures(1, &g_format_tex);
    }
    if (g_format_fbo)
    {
        glDeleteFramebuffers(1, &g_format_fbo);
    }

    free(g_flip_buf);

    g_flip_buf    = nullptr;
    g_flip_buf_sz = 0;
    g_program     = 0;
    g_texture     = 0;
    g_format_tex  = 0;
    g_format_fbo  = 0;
    g_quad_vao    = 0;
}

void format_pass_upload(const uint8_t * data)
{
    const uint8_t * upload = flip_for_upload(data);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_width, g_height, g_format.gl_format, g_format.gl_type, upload);

}

void format_pass_render_to_fbo(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, g_format_fbo);
    format_pass_render_to_bound_fbo();
}

void format_pass_render_to_bound_fbo(void)
{
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

    glBindVertexArray(g_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

GLuint format_pass_get_output_tex(void)
{
    return g_format_tex;
}

int format_pass_get_bytes_per_pixel(void)
{
    return g_format.bytes_per_pixel;
}
