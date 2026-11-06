#include "svg_icons.h"

#include "log/logger.h"
#include "render/gl_api.h"

#include <stdlib.h>
#include <string.h>

// Отключаем предупреждения внутри сторонних библиотек
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wshift-negative-value"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"

#define NANOVG_GL3_IMPLEMENTATION
#define NANOVG_GL_USE_GLFW
#include "nanovg.h"
#include "nanovg_gl.h"

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

#pragma GCC diagnostic pop

#define ICON_RASTER_SIZE 96 // Разрешение растеризации (с запасом под DPI)

//------------------------------------------------------- SVG-иконки ---------------------------------------------------

static const char svg_stats[] = {
#embed "../../res/icons/stats.svg" suffix(, 0)
};
static const char svg_help[] = {
#embed "../../res/icons/help.svg" suffix(, 0)
};
static const char svg_smoothing[] = {
#embed "../../res/icons/smoothing.svg" suffix(, 0)
};
static const char svg_auto_levels[] = {
#embed "../../res/icons/auto_levels.svg" suffix(, 0)
};
static const char svg_saturation[] = {
#embed "../../res/icons/saturation.svg" suffix(, 0)
};
static const char svg_histogram[] = {
#embed "../../res/icons/histogram.svg" suffix(, 0)
};
static const char svg_log_scale[] = {
#embed "../../res/icons/log_scale.svg" suffix(, 0)
};
static const char svg_line_profile[] = {
#embed "../../res/icons/line_profile.svg" suffix(, 0)
};
static const char svg_gradient[] = {
#embed "../../res/icons/gradient.svg" suffix(, 0)
};
static const char svg_inspector[] = {
#embed "../../res/icons/inspector.svg" suffix(, 0)
};
static const char svg_diff_view[] = {
#embed "../../res/icons/diff_view.svg" suffix(, 0)
};
static const char svg_set_reference[] = {
#embed "../../res/icons/set_reference.svg" suffix(, 0)
};
static const char svg_diff_gain_down[] = {
#embed "../../res/icons/diff_gain_down.svg" suffix(, 0)
};
static const char svg_diff_gain_up[] = {
#embed "../../res/icons/diff_gain_up.svg" suffix(, 0)
};
static const char svg_pause[] = {
#embed "../../res/icons/pause.svg" suffix(, 0)
};
static const char svg_forward[] = {
#embed "../../res/icons/forward.svg" suffix(, 0)
};
static const char svg_backward[] = {
#embed "../../res/icons/backward.svg" suffix(, 0)
};
static const char svg_recording[] = {
#embed "../../res/icons/recording.svg" suffix(, 0)
};
static const char svg_screenshot[] = {
#embed "../../res/icons/screenshot.svg" suffix(, 0)
};
static const char svg_flush_save[] = {
#embed "../../res/icons/flush_save.svg" suffix(, 0)
};
static const char svg_clear_selection[] = {
#embed "../../res/icons/clear_selection.svg" suffix(, 0)
};
static const char svg_reset_zoom[] = {
#embed "../../res/icons/reset_zoom.svg" suffix(, 0)
};
static const char svg_fullscreen[] = {
#embed "../../res/icons/fullscreen.svg" suffix(, 0)
};
static const char svg_vectorscope[] = {
#embed "../../res/icons/vectorscope.svg" suffix(, 0)
};

//------------------------------------------------------- Таблица -----------------------------------------------------

typedef struct
{
    commands_e cmd;
    const char * svg;
} icon_entry_t;

static const icon_entry_t g_icon_table[] = {
    {CMD_TOGGLE_STATS,           svg_stats          },
    {CMD_SHOW_HELP,              svg_help           },
    {CMD_TOGGLE_SMOOTHING,       svg_smoothing      },
    {CMD_TOGGLE_AUTO_LEVELS,     svg_auto_levels    },
    {CMD_TOGGLE_SATURATION,      svg_saturation     },
    {CMD_TOGGLE_HISTOGRAM,       svg_histogram      },
    {CMD_TOGGLE_HISTOGRAM_SCALE, svg_log_scale      },
    {CMD_TOGGLE_LINE_PROFILE,    svg_line_profile   },
    {CMD_TOGGLE_GRADIENT,        svg_gradient       },
    {CMD_TOGGLE_INSPECTOR,       svg_inspector      },
    {CMD_TOGGLE_DIFF,            svg_diff_view      },
    {CMD_TOGGLE_VECTORSCOPE,     svg_vectorscope    },
    {CMD_SET_REFERENCE,          svg_set_reference  },
    {CMD_DIFF_GAIN_DOWN,         svg_diff_gain_down },
    {CMD_DIFF_GAIN_UP,           svg_diff_gain_up   },
    {CMD_TOGGLE_PAUSE,           svg_pause          },
    {CMD_STEP_FORWARD,           svg_forward        },
    {CMD_STEP_BACKWARD,          svg_backward       },
    {CMD_TOGGLE_RECORDING,       svg_recording      },
    {CMD_REQUEST_SCREENSHOT,     svg_screenshot     },
    {CMD_FLUSH_SAVER,            svg_flush_save     },
    {CMD_CLEAR_SELECTION,        svg_clear_selection},
    {CMD_ZOOM_RESET,             svg_reset_zoom     },
    {CMD_TOGGLE_FULLSCREEN,      svg_fullscreen     },
};

#define ICON_COUNT ((int)(sizeof(g_icon_table) / sizeof(g_icon_table[ 0 ])))

//------------------------------------------------------- Состояние ---------------------------------------------------

static NVGcontext * g_nvg           = nullptr;
static NSVGrasterizer * g_rast      = nullptr;
static unsigned char * g_raster_buf = nullptr;

// -2 = не инициализирована, -1 = ошибка (не повторяем), >=0 = NanoVG handle
static int g_icon_handles[ ICON_COUNT ];

//------------------------------------------------------- Ленивая загрузка ---------------------------------------------

static int find_icon_index(commands_e cmd)
{
    for (int i = 0; i < ICON_COUNT; i++)
    {
        if (g_icon_table[ i ].cmd == cmd)
        {
            return i;
        }
    }
    return -1;
}

/// \brief Растеризует SVG при первом обращении и кеширует текстуру NanoVG.
static int ensure_icon_loaded(int idx)
{
    if (idx < 0 || idx >= ICON_COUNT)
    {
        return -1;
    }
    if (g_icon_handles[ idx ] != -2)
    {
        return g_icon_handles[ idx ];
    }

    const char * src = g_icon_table[ idx ].svg;
    if (!src || !g_rast || !g_nvg)
    {
        g_icon_handles[ idx ] = -1;
        return -1;
    }

    char * buf = (char *)malloc(strlen(src) + 1);
    if (!buf)
    {
        g_icon_handles[ idx ] = -1;
        return -1;
    }
    strcpy(buf, src);
    NSVGimage * image = nsvgParse(buf, "px", 96.0f);
    free(buf);

    if (!image)
    {
        log_warn("svg_icons: failed to parse SVG for cmd=%d", (int)g_icon_table[ idx ].cmd);
        g_icon_handles[ idx ] = -1;
        return -1;
    }

    for (NSVGshape * shape = image->shapes; shape != nullptr; shape = shape->next)
    {
        shape->fill.type   = NSVG_PAINT_COLOR;
        shape->fill.color  = 0xFFFFFFFF;
        shape->stroke.type = NSVG_PAINT_NONE;
    }

    memset(g_raster_buf, 0, ICON_RASTER_SIZE * ICON_RASTER_SIZE * 4);

    float max_dim = (image->width > image->height) ? image->width : image->height;
    float scale   = (max_dim > 0.0f) ? ((float)ICON_RASTER_SIZE / max_dim) : 1.0f;

    nsvgRasterize(g_rast, image, 0, 0, scale, g_raster_buf, ICON_RASTER_SIZE, ICON_RASTER_SIZE, ICON_RASTER_SIZE * 4);

    g_icon_handles[ idx ] = nvgCreateImageRGBA(g_nvg, ICON_RASTER_SIZE, ICON_RASTER_SIZE, 0, g_raster_buf);

    nsvgDelete(image);
    return g_icon_handles[ idx ];
}

//------------------------------------------------------- API ---------------------------------------------------------

bool svg_icons_init(void)
{
    g_nvg = nvgCreateGL3(NVG_ANTIALIAS);
    if (!g_nvg)
    {
        log_error("svg_icons: nvgCreateGL3 failed");
        return false;
    }

    g_rast = nsvgCreateRasterizer();
    if (!g_rast)
    {
        log_error("svg_icons: nsvgCreateRasterizer failed");
        nvgDeleteGL3(g_nvg);
        g_nvg = nullptr;
        return false;
    }

    g_raster_buf = (unsigned char *)malloc(ICON_RASTER_SIZE * ICON_RASTER_SIZE * 4);
    if (!g_raster_buf)
    {
        log_error("svg_icons: out of memory for raster buffer");
        nsvgDeleteRasterizer(g_rast);
        g_rast = nullptr;
        nvgDeleteGL3(g_nvg);
        g_nvg = nullptr;
        return false;
    }

    for (int i = 0; i < ICON_COUNT; i++)
    {
        g_icon_handles[ i ] = -2; // не загружена
    }

    log_info("svg_icons: NanoVG initialised (lazy icons, %d entries)", ICON_COUNT);
    return true;
}

void svg_icons_cleanup(void)
{
    if (g_nvg)
    {
        nvgDeleteGL3(g_nvg);
        g_nvg = nullptr;
    }
    if (g_rast)
    {
        nsvgDeleteRasterizer(g_rast);
        g_rast = nullptr;
    }
    free(g_raster_buf);
    g_raster_buf = nullptr;

    for (int i = 0; i < ICON_COUNT; i++)
    {
        g_icon_handles[ i ] = -2;
    }
}

void svg_icons_begin(image_size_s win, float dpi_scale)
{
    if (!g_nvg)
    {
        return;
    }
    (void)dpi_scale;
    nvgBeginFrame(g_nvg, (float)win.x, (float)win.y, 1.0f);
}

void svg_icons_end(void)
{
    if (!g_nvg)
    {
        return;
    }
    nvgEndFrame(g_nvg);

    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(0);
    glBindVertexArray(0);
}

bool svg_icon_exists(commands_e cmd)
{
    return find_icon_index(cmd) >= 0;
}

bool svg_icons_draw(commands_e cmd, point_s center, float size, fcolor_rgba_s color)
{
    if (!g_nvg || !g_rast)
    {
        return false;
    }

    int idx = find_icon_index(cmd);
    if (idx < 0)
    {
        return false;
    }

    int handle = ensure_icon_loaded(idx);
    if (handle < 0)
    {
        return false;
    }

    (void)color; // иконки растеризованы белыми

    float x0 = center.x - size * 0.5f;
    float y0 = center.y - size * 0.5f;

    NVGpaint paint = nvgImagePattern(g_nvg, x0, y0, size, size, 0.0f, handle, 1.0f);

    nvgBeginPath(g_nvg);
    nvgRect(g_nvg, x0, y0, size, size);
    nvgFillPaint(g_nvg, paint);
    nvgFill(g_nvg);

    return true;
}
