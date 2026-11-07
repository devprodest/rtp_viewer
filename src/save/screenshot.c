#include "screenshot.h"
#include "log/logger.h"
#include "render/format_pass.h"
#include "render/gl/gl_api.h"
#include "png_writer.h"
#include "saver.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


static bool g_requested = false;
static GLuint g_fbo     = 0;
static GLuint g_tex     = 0;

static int g_width  = 0;
static int g_height = 0;

static bool ensure_fbo(void)
{
    if (g_fbo)
    {
        return true;
    }

    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_width, g_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &g_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        log_warn("screenshot FBO incomplete: 0x%x\n", status);
        if (g_tex)
        {
            glDeleteTextures(1, &g_tex);
            g_tex = 0;
        }
        if (g_fbo)
        {
            glDeleteFramebuffers(1, &g_fbo);
            g_fbo = 0;
        }
        return false;
    }
    return true;
}

void screenshot_init(void)
{
    g_requested = false;
}

void screenshot_cleanup(void)
{
    if (g_tex)
    {
        glDeleteTextures(1, &g_tex);
        g_tex = 0;
    }
    if (g_fbo)
    {
        glDeleteFramebuffers(1, &g_fbo);
        g_fbo = 0;
    }
    g_requested = false;
}

void screenshot_request(void)
{
    g_requested = true;
}

void screenshot_set_size(int width, int height)
{
    g_width  = width;
    g_height = height;
}

void screenshot_process_if_requested(void)
{
    if (!g_requested)
    {
        return;
    }
    g_requested = false;

    if (!ensure_fbo())
    {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    format_pass_render_to_bound_fbo();

    size_t stride = (size_t)g_width * 4;
    void * pixels = malloc(stride * (size_t)g_height);
    if (!pixels)
    {
        log_warn("screenshot: out of memory\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, g_width, g_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    time_t t           = time(nullptr);
    struct tm * tm_now = localtime(&t);

    char stamp[ 32 ];
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", tm_now);

    const char * outdir = saver_get_outdir();
    char path[ 1024 ];
    snprintf(path, sizeof(path), "%s/screenshot_%s.png", outdir, stamp);

    if (png_write_rgba(path, pixels, g_width, g_height))
    {
        log_info("Screenshot saved: %s (%dx%d)\n", path, g_width, g_height);
    }
    else
    {
        log_warn("Failed to save screenshot: %s\n", path);
    }

    free(pixels);
}
