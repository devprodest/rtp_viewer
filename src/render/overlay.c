/// \file overlay.c
/// \brief Отрисовка текста и простых фигур поверх GL-сцены.
///
/// \details
/// Текст рендерится через stb_truetype: TTF встроен в бинарник через #embed,
/// при старте запекается в атлас 512x512 (ASCII 32..127), затем каждый символ
/// рисуется как текстурированный квад с альфа-смешиванием.

#include "overlay.h"

#include "gl/gl_api.h"
#include "gl/gl_utils.h"
#include "log/logger.h"
#include "util/utils.h"

#include <stdlib.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#pragma GCC diagnostic pop

//------------------------------------------------------ Macros --------------------------------------------------------

#define FONT_ATLAS_W       256
#define FONT_ATLAS_H       256
#define FONT_BAKE_SIZE     18.0f
#define FONT_FIRST_CHAR    32
#define FONT_NUM_CHARS     96

#define TEXT_MAX_VERTS     100000
#define TEXT_INITIAL_VERTS 4096

#define WHITE_U            (0.5f / FONT_ATLAS_W)
#define WHITE_V            (0.5f / FONT_ATLAS_H)

//----------------------------------------------------- Typedefs -------------------------------------------------------

typedef struct text_vertex_s
{
    fpoint_s pos;
    fcolor_rgba_s color;
    fpoint_s uv;
} text_vertex_t;

//---------------------------------------------------- Variables -------------------------------------------------------

static text_vertex_t * g_verts = nullptr;
static int g_vert_count        = 0;
static int g_vert_capacity     = 0;

static fimage_size_s g_win = {1.0f, 1.0f};

static GLuint g_vao     = 0;
static GLuint g_vbo     = 0;
static GLuint g_program = 0;

static float g_font_ascent = 0.0f;

// Шрифт
static unsigned char g_font_data[] = {
#embed "../res/RobotoMono-Regular.ttf"
};

static stbtt_fontinfo g_font_info;
static stbtt_bakedchar g_baked_chars[ FONT_NUM_CHARS ];
static GLuint g_font_texture = 0;

//------------------------------------------------- Static Functions ---------------------------------------------------

static bool ensure_vertex_capacity(int need)
{
    if (g_vert_capacity >= need)
    {
        return true;
    }

    int new_cap = g_vert_capacity ? g_vert_capacity * 2 : TEXT_INITIAL_VERTS;
    if (new_cap < need)
    {
        new_cap = need;
    }
    if (new_cap > TEXT_MAX_VERTS)
    {
        new_cap = TEXT_MAX_VERTS;
    }
    if (new_cap < need)
    {
        return false;
    }

    text_vertex_t * nv = realloc(g_verts, (size_t)new_cap * sizeof(text_vertex_t));
    if (!nv)
    {
        return false;
    }

    g_verts         = nv;
    g_vert_capacity = new_cap;
    return true;
}

static void emit_quad_ndc(frect_s rect, fcolor_rgba_s color, fpoint_s uv0, fpoint_s uv1)
{
    if (!ensure_vertex_capacity(g_vert_count + 6))
    {
        return;
    }

    float nx0 = (rect.x0 / g_win.x) * 2.0f - 1.0f;
    float nx1 = (rect.x1 / g_win.x) * 2.0f - 1.0f;
    float ny0 = 1.0f - (rect.y0 / g_win.y) * 2.0f;
    float ny1 = 1.0f - (rect.y1 / g_win.y) * 2.0f;

    text_vertex_t * v = &g_verts[ g_vert_count ];

    v[ 0 ] = (text_vertex_t) {
        .pos = {nx0,   ny0  },
          .color = color, .uv = {uv0.x, uv0.y}
    };
    v[ 1 ] = (text_vertex_t) {
        .pos = {nx1,   ny0  },
          .color = color, .uv = {uv1.x, uv0.y}
    };
    v[ 2 ] = (text_vertex_t) {
        .pos = {nx1,   ny1  },
          .color = color, .uv = {uv1.x, uv1.y}
    };

    v[ 3 ] = (text_vertex_t) {
        .pos = {nx0,   ny0  },
          .color = color, .uv = {uv0.x, uv0.y}
    };
    v[ 4 ] = (text_vertex_t) {
        .pos = {nx1,   ny1  },
          .color = color, .uv = {uv1.x, uv1.y}
    };
    v[ 5 ] = (text_vertex_t) {
        .pos = {nx0,   ny1  },
          .color = color, .uv = {uv0.x, uv1.y}
    };

    g_vert_count += 6;
}

//---------------------------------------------------- Functions -------------------------------------------------------

void overlay_init(void)
{
    // Инициализация шрифта
    if (!stbtt_InitFont(&g_font_info, g_font_data, 0))
    {
        log_error("overlay: stbtt_InitFont failed");
        return;
    }

    // Вычисляем верхнюю границу шрифта для правильного позиционирования
    float scale_font = stbtt_ScaleForPixelHeight(&g_font_info, FONT_BAKE_SIZE);
    int ascent       = 0;
    int descent      = 0;
    int lineGap      = 0;
    stbtt_GetFontVMetrics(&g_font_info, &ascent, &descent, &lineGap);
    g_font_ascent = ascent * scale_font;

    unsigned char * bitmap = malloc(FONT_ATLAS_W * FONT_ATLAS_H);
    if (!bitmap)
    {
        log_error("overlay: font bitmap alloc failed");
        return;
    }

    int result = stbtt_BakeFontBitmap(g_font_data, 0, FONT_BAKE_SIZE, bitmap, FONT_ATLAS_W, FONT_ATLAS_H,
                                      FONT_FIRST_CHAR, FONT_NUM_CHARS, g_baked_chars);

    if (result <= 0)
    {
        log_error("overlay: stbtt_BakeFontBitmap failed (atlas too small?)");
        free(bitmap);
        return;
    }

    bitmap[ 0 ] = 255; // непрозрачный белый пиксель для прямоугольников


    glGenTextures(1, &g_font_texture);
    glBindTexture(GL_TEXTURE_2D, g_font_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, FONT_ATLAS_W, FONT_ATLAS_H, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(bitmap);

    // Шейдеры
    static const char * vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 aPos;\n"
        "layout(location=1) in vec4 aColor;\n"
        "layout(location=2) in vec2 aUV;\n"
        "out vec4 vColor;\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    vColor = aColor;\n"
        "    vUV = aUV;\n"
        "}\n";

    static const char * fs =
        "#version 330 core\n"
        "in vec4 vColor;\n"
        "in vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "    float alpha = texture(uTex, vUV).r;\n"
        "    FragColor = vec4(vColor.rgb, vColor.a * alpha);\n"
        "}\n";

    GLuint v = gl_utils_compile_shader(vs, GL_VERTEX_SHADER);
    GLuint f = gl_utils_compile_shader(fs, GL_FRAGMENT_SHADER);
    if (!v || !f)
    {
        if (v)
        {
            glDeleteShader(v);
        }
        if (f)
        {
            glDeleteShader(f);
        }
        return;
    }

    g_program = glCreateProgram();
    glAttachShader(g_program, v);
    glAttachShader(g_program, f);
    glLinkProgram(g_program);
    glDeleteShader(v);
    glDeleteShader(f);

    GLint ok = 0;
    glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char info[ 512 ];
        glGetProgramInfoLog(g_program, sizeof(info), nullptr, info);
        log_error("overlay: link error: %s", info);
        glDeleteProgram(g_program);
        g_program = 0;
        return;
    }

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    ensure_vertex_capacity(TEXT_INITIAL_VERTS);
}

void overlay_cleanup(void)
{
    if (g_vbo)
    {
        glDeleteBuffers(1, &g_vbo);
        g_vbo = 0;
    }
    if (g_vao)
    {
        glDeleteVertexArrays(1, &g_vao);
        g_vao = 0;
    }
    if (g_program)
    {
        glDeleteProgram(g_program);
        g_program = 0;
    }
    if (g_font_texture)
    {
        glDeleteTextures(1, &g_font_texture);
        g_font_texture = 0;
    }

    free(g_verts);
    g_verts         = nullptr;
    g_vert_capacity = 0;
    g_vert_count    = 0;
}

void overlay_begin(image_size_s win)
{
    g_win.x      = win.x > 0 ? win.x : 1;
    g_win.y      = win.y > 0 ? win.y : 1;
    g_vert_count = 0;
}

void overlay_text_draw(point_s s, const char * text, fcolor_rgb_s color, float scale)
{
    if (!text || !*text || scale <= 0.0f)
    {
        return;
    }

    float xpos_font = (float)s.x / scale;
    float ypos_font = (float)s.y / scale + g_font_ascent; // сдвиг базовой линии вниз

    fcolor_rgba_s rgba = {color.r, color.g, color.b, 1.0f};

    for (const char * p = text; *p; p++)
    {
        int index = (unsigned char)*p - FONT_FIRST_CHAR;
        if (index < 0 || index >= FONT_NUM_CHARS)
        {
            continue;
        }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(g_baked_chars, FONT_ATLAS_W, FONT_ATLAS_H, index, &xpos_font, &ypos_font, &q, 1);

        frect_s rect = {
            .x0 = q.x0 * scale,
            .y0 = q.y0 * scale,
            .x1 = q.x1 * scale,
            .y1 = q.y1 * scale,
        };
        fpoint_s uv0 = {q.s0, q.t0};
        fpoint_s uv1 = {q.s1, q.t1};

        emit_quad_ndc(rect, rgba, uv0, uv1);
    }
}

int overlay_text_measure(const char * text, float scale)
{
    if (!text || !*text || scale <= 0.0f)
    {
        return 0;
    }

    float width = 0.0f;
    for (const char * p = text; *p; p++)
    {
        int index = (unsigned char)*p - FONT_FIRST_CHAR;
        if (index < 0 || index >= FONT_NUM_CHARS)
        {
            continue;
        }
        width += g_baked_chars[ index ].xadvance;
    }
    return (int)(width * scale + 0.5f);
}

void overlay_rect_draw(rect_s rect, fcolor_rgba_s color)
{
    fpoint_s uv = {WHITE_U, WHITE_V};
    emit_quad_ndc(rect_convert(frect_s, rect), color, uv, uv);
}

void overlay_rect_outline_draw(rect_s rect, fcolor_rgba_s color, int thickness)
{
    fpoint_s uv = {WHITE_U, WHITE_V};
    if (thickness < 1)
    {
        thickness = 1;
    }

    emit_quad_ndc((frect_s) {rect.x0, rect.y0, rect.x1, rect.y0 + thickness}, color, uv, uv);
    emit_quad_ndc((frect_s) {rect.x0, rect.y1 - thickness, rect.x1, rect.y1}, color, uv, uv);
    emit_quad_ndc((frect_s) {rect.x0, rect.y0, rect.x0 + thickness, rect.y1}, color, uv, uv);
    emit_quad_ndc((frect_s) {rect.x1 - thickness, rect.y0, rect.x1, rect.y1}, color, uv, uv);
}

void overlay_end(void)
{
    if (g_vert_count == 0 || !g_program || !g_vao || !g_font_texture)
    {
        return;
    }

    glViewport(0, 0, (int)g_win.x, (int)g_win.y);

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_font_texture);
    glUniform1i(glGetUniformLocation(g_program, "uTex"), 0);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)g_vert_count * sizeof(text_vertex_t)), g_verts, GL_DYNAMIC_DRAW);

    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, g_vert_count);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
