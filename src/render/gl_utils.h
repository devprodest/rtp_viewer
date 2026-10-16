#pragma once

#include "gl_api.h"

GLuint gl_utils_compile_shader(const char * source, GLenum type);
GLuint gl_utils_build_program(const char * fragment_source);
GLuint gl_utils_create_fullscreen_quad(void);


#define gl_min(val, min)              \
    ({                                \
        __typeof__(val) mval = (val); \
        __typeof__(min) mmin = (min); \
        (mval < mmin) ? mval : mmin;  \
    })

#define gl_max(val, max)                 \
    ({                                   \
        __typeof__(val) mxval = (val);   \
        __typeof__(max) mxmax = (max);   \
        (mxval > mxmax) ? mxval : mxmax; \
    })


#define gl_clamp(val, min, max)       \
    ({                                \
        __typeof__(val) cval = (val); \
        __typeof__(max) cmax = (max); \
        __typeof__(min) cmin = (min); \
                                      \
        cval = gl_max(cval, cmin);    \
        cval = gl_min(cval, cmax);    \
        cval;                         \
    })
