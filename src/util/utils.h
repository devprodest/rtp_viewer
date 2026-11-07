#pragma once

#include "types.h"
#include <stddef.h>
#include <stdint.h>

uint64_t get_time_us(void);
uint64_t get_time_s(void);
char * read_file(const char * path);

int parse_int(const char * s, long min, long max, long * out);


bool utils_build_filename(char * out, size_t outsz, const char * dir, const char * prefix, time_t timestamp,
                          image_size_s image, const char * format, const char * suffix, const char * ext);

#define utils_min(val, min)           \
    ({                                \
        __typeof__(val) mval = (val); \
        __typeof__(min) mmin = (min); \
        (mval < mmin) ? mval : mmin;  \
    })

#define utils_max(val, max)              \
    ({                                   \
        __typeof__(val) mxval = (val);   \
        __typeof__(max) mxmax = (max);   \
        (mxval > mxmax) ? mxval : mxmax; \
    })


#define utils_clamp(val, min, max)    \
    ({                                \
        __typeof__(val) cval = (val); \
        __typeof__(max) cmax = (max); \
        __typeof__(min) cmin = (min); \
                                      \
        cval = utils_max(cval, cmin); \
        cval = utils_min(cval, cmax); \
        cval;                         \
    })

#define utils_abs(val)                \
    ({                                \
        __typeof__(val) aval = (val); \
        (aval < 0) ? -aval : aval;    \
    })
