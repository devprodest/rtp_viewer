#include "format_config.h"

#include "log/logger.h"
#include "render/gl_api.h"
#include "util/utils.h"

#include <stdio.h>
#include <string.h>


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
    if (strcmp(value, "GL_RGB8") == 0)
    {
        return GL_RGB8;
    }
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

bool format_config_load(const char * format_name, format_config_s * cfg)
{
    char path[ 256 ];
    snprintf(path, sizeof(path), "formats/%s.fmt", format_name);

    FILE * f = fopen(path, "r");
    if (!f)
    {
        log_error("format: Cannot open format file: %s\n", path);
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
            long bpp = 0;
            if (parse_int(value, 1, 128, &bpp) == 0)
            {
                cfg->bytes_per_pixel = (int)bpp;
            }
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
                log_error("format: Unknown internal format: %s\n", value);
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
                log_error("format: Unknown format: %s\n", value);
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
                log_error("format: Unknown type: %s\n", value);
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
