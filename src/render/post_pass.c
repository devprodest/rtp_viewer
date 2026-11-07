#include "post_pass.h"
#include "analysis/analytics.h"
#include "log/logger.h"
#include "render/ui/window_title.h"
#include "util/utils.h"
#include "gl_utils.h"

#include <stddef.h>
#include <stdio.h>

static char POST_SHADER_FRAGMENT[] = {
#embed "../res/post.glsl" suffix(, 0)
};

static GLuint g_program            = 0;
static GLuint g_quad_vao           = 0;
static GLint g_uniform_tex         = -1;
static GLint g_uniform_saturation  = -1;
static GLint g_uniform_gradient    = -1;
static GLint g_uniform_texel_size  = -1;
static GLint g_uniform_origin      = -1;
static GLint g_uniform_size        = -1;
static GLint g_uniform_auto_levels = -1;
static GLint g_uniform_level_min   = -1;
static GLint g_uniform_level_max   = -1;

static fimage_size_s g_texel = {};
static int g_bpp             = 3;
static int g_eff_bits        = 8;

static bool g_saturation_enabled  = false;
static bool g_gradient_enabled    = false;
static bool g_auto_levels_enabled = false;
static float g_level_min[ 3 ]     = {0.0f, 0.0f, 0.0f};
static float g_level_max[ 3 ]     = {1.0f, 1.0f, 1.0f};

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

void post_pass_set_format(int bytes_per_pixel, int effective_bits)
{
    g_bpp      = bytes_per_pixel;
    g_eff_bits = effective_bits;
}

void post_pass_set_texel_size(fimage_size_s size)
{
    g_texel = size;
}

void post_pass_toggle_saturation(void)
{
    g_saturation_enabled = !g_saturation_enabled;
    log_info("Saturation markers %s", g_saturation_enabled ? "enabled" : "disabled");
    window_title_refresh();
}
bool post_pass_is_saturation_active(void)
{
    return g_saturation_enabled;
}

void post_pass_toggle_gradient(void)
{
    g_gradient_enabled = !g_gradient_enabled;
    if (!g_gradient_enabled)
    {
        window_title_set_sharpness(false, 0.0, 0.0);
    }
    log_info("Gradient %s", g_gradient_enabled ? "enabled" : "disabled");
    window_title_refresh();
}
bool post_pass_is_gradient_active(void)
{
    return g_gradient_enabled;
}

void post_pass_toggle_auto_levels(void)
{
    g_auto_levels_enabled = !g_auto_levels_enabled;
    log_info("Auto-levels %s", g_auto_levels_enabled ? "enabled" : "disabled");
}
bool post_pass_is_auto_levels_active(void)
{
    return g_auto_levels_enabled;
}

static void update_levels_from_stats(void)
{
    if (!g_auto_levels_enabled)
    {
        return;
    }

    const analytics_stats_s * st = analytics_get_stats();
    float lmin[ 3 ]              = {0.0f, 0.0f, 0.0f};
    float lmax[ 3 ]              = {1.0f, 1.0f, 1.0f};

    if (st && st->valid)
    {
        const double max_range = (g_bpp == 2) ? (double)((1u << g_eff_bits) - 1u) : 255.0;

        for (int c = 0; c < 3; c++)
        {
            double lo = st->min[ c ] / max_range;
            double hi = st->max[ c ] / max_range;
            if (hi <= lo + 1e-3)
            {
                lo = 0.0;
                hi = 1.0;
            }
            lmin[ c ] = (float)utils_clamp(lo, 0.0, 1.0);
            lmax[ c ] = (float)utils_clamp(hi, 0.0, 1.0);
        }
        if (st->channels == 1)
        {
            lmin[ 1 ] = lmin[ 2 ] = lmin[ 0 ];
            lmax[ 1 ] = lmax[ 2 ] = lmax[ 0 ];
        }
    }

    for (int i = 0; i < 3; i++)
    {
        g_level_min[ i ] = lmin[ i ];
        g_level_max[ i ] = lmax[ i ];
    }
}

void post_pass_render(GLuint source_tex, point_s dst, image_size_s size, image_size_s win)
{
    update_levels_from_stats();

    int gl_y = win.y - dst.y - size.y;
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
