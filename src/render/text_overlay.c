/// \file text_overlay.c
/// \brief Отрисовка ASCII-текста поверх GL-сцены через stb_easy_font.
/// \date 2026
///
/// \details
/// stb_easy_font строит текст из квадов. Мы преобразуем их в треугольники
/// (GL 3.3 core не знает GL_QUADS), переводим координаты из пикселей окна
/// в NDC и рисуем одним glDrawArrays.
///
/// Атрибуты: position (vec2, NDC), color (vec4, RGBA float).

#include "text_overlay.h"
#include "gl_api.h"
#include "log/logger.h"

#include "log/logger.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "stb_easy_font.h"

//------------------------------------------------------ Macros --------------------------------------------------------

/// \brief Размер одного vertex-элемента stb_easy_font.
///
/// \details Формат: float x, y, z; uint8 color[4]. Итого 16 байт.
///          Менять не нужно, определяется stb_easy_font.h.
#define TEXT_STB_VERTEX_BYTES   16

/// \brief Вершин на квад в формате stb_easy_font.
#define TEXT_STB_VERTS_PER_QUAD 4

/// \brief Максимум вершин в одном кадре.
///
/// \details 6 вершин на квад × ~50 квадов на строку × ~300 строк.
///          При 24 байтах на вершину = 43 МБ — с большим запасом.
///          Допустимый диапазон: 4096..500000. По умолчанию: 100000.
#define TEXT_MAX_VERTS          100000

/// \brief Стартовая ёмкость буфера вершин.
///
/// \details Растёт при необходимости до TEXT_MAX_VERTS.
#define TEXT_INITIAL_VERTS      4096

/// \brief Размер scratch-буфера под одну строку для stb_easy_font_print.
///
/// \details stb_easy_font потребляет ~270 байт на символ. 64 КБ хватает
///          на ~200 символов одной строки.
#define TEXT_SCRATCH_SIZE       65536

//----------------------------------------------------- Typedefs -------------------------------------------------------

/// \brief Вершина текста: NDC-координаты и цвет.
typedef struct text_vertex_s
{
    float x; ///< X в NDC [-1..1]
    float y; ///< Y в NDC [-1..1]
    float r; ///< Красный 0..1
    float g; ///< Зелёный 0..1
    float b; ///< Синий 0..1
    float a; ///< Альфа 0..1
} text_vertex_t;

//---------------------------------------------------- Variables -------------------------------------------------------

static text_vertex_t * g_verts = NULL; ///< Накопленные вершины кадра
static int g_vert_count        = 0;    ///< Текущее число вершин
static int g_vert_capacity     = 0;    ///< Ёмкость буфера

static int g_win_w = 1; ///< Ширина окна, пиксели
static int g_win_h = 1; ///< Высота окна, пиксели

static GLuint g_vao     = 0; ///< VAO для атрибутов
static GLuint g_vbo     = 0; ///< VBO с вершинами
static GLuint g_program = 0; ///< Шейдерная программа

static char g_scratch[ TEXT_SCRATCH_SIZE ]; ///< Буфер под строку для stb_easy_font

//------------------------------------------------- Static Functions ---------------------------------------------------

/// \brief Расширяет буфер вершин при необходимости.
///
/// \param need Требуемое число вершин
///
/// \return true, если места достаточно (после возможного расширения)
static bool ensure_vertex_capacity(int need)
{
    if (g_vert_capacity >= need)
    {
        return true;
    }

    int new_cap = g_vert_capacity ? (g_vert_capacity * 2) : TEXT_INITIAL_VERTS;
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

/// \brief Компилирует один шейдер.
///
/// \param source Исходник
/// \param type   GL_VERTEX_SHADER или GL_FRAGMENT_SHADER
///
/// \return ID шейдера или 0
static GLuint compile_shader(const char * source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char info[ 512 ];
        glGetShaderInfoLog(shader, sizeof(info), NULL, info);
        log_error("text_overlay shader error: %s\n", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

/// \brief Выдаёт один квад как два треугольника в NDC.
///
/// \param x0 Левая граница в пикселях окна (top-left origin)
/// \param y0 Верхняя граница
/// \param x1 Правая граница
/// \param y1 Нижняя граница
/// \param r  Красный 0..1
/// \param g  Зелёный 0..1
/// \param b  Синий 0..1
/// \param a  Альфа 0..1
static void emit_quad_ndc(float x0, float y0, float x1, float y1, float r, float g, float b, float a)
{
    if (!ensure_vertex_capacity(g_vert_count + 6))
    {
        return;
    }

    float nx0 = (x0 / (float)g_win_w) * 2.0f - 1.0f;
    float nx1 = (x1 / (float)g_win_w) * 2.0f - 1.0f;
    float ny0 = 1.0f - (y0 / (float)g_win_h) * 2.0f;
    float ny1 = 1.0f - (y1 / (float)g_win_h) * 2.0f;

    text_vertex_t * v = &g_verts[ g_vert_count ];

    v[ 0 ] = (text_vertex_t) {nx0, ny0, r, g, b, a};
    v[ 1 ] = (text_vertex_t) {nx1, ny0, r, g, b, a};
    v[ 2 ] = (text_vertex_t) {nx1, ny1, r, g, b, a};

    v[ 3 ] = (text_vertex_t) {nx0, ny0, r, g, b, a};
    v[ 4 ] = (text_vertex_t) {nx1, ny1, r, g, b, a};
    v[ 5 ] = (text_vertex_t) {nx0, ny1, r, g, b, a};

    g_vert_count += 6;
}

//---------------------------------------------------- Functions -------------------------------------------------------

void text_overlay_init(void)
{
    static const char * vs =
        "#version 330 core\n"
        "layout(location=0) in vec2 aPos;\n"
        "layout(location=1) in vec4 aColor;\n"
        "out vec4 vColor;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    vColor = aColor;\n"
        "}\n";

    static const char * fs =
        "#version 330 core\n"
        "in vec4 vColor;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "    FragColor = vColor;\n"
        "}\n";

    GLuint v = compile_shader(vs, GL_VERTEX_SHADER);
    if (!v)
    {
        return;
    }
    GLuint f = compile_shader(fs, GL_FRAGMENT_SHADER);
    if (!f)
    {
        glDeleteShader(v);
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
        glGetProgramInfoLog(g_program, sizeof(info), NULL, info);
        log_error("text_overlay link error: %s\n", info);
        glDeleteProgram(g_program);
        g_program = 0;
        return;
    }

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(text_vertex_t), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    ensure_vertex_capacity(TEXT_INITIAL_VERTS);
}

void text_overlay_cleanup(void)
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

    free(g_verts);
    g_verts         = NULL;
    g_vert_capacity = 0;
    g_vert_count    = 0;
}

void text_begin(int win_w, int win_h)
{
    g_win_w      = win_w > 0 ? win_w : 1;
    g_win_h      = win_h > 0 ? win_h : 1;
    g_vert_count = 0;
}

void text_draw(int x, int y, const char * text, float r, float g, float b, float scale)
{
    if (!text || !*text || scale <= 0.0f)
    {
        return;
    }

    int num_quads = stb_easy_font_print(0.0f, 0.0f, (char *)text, NULL, g_scratch, sizeof(g_scratch));
    if (num_quads <= 0)
    {
        return;
    }

    const uint8_t * p = (const uint8_t *)g_scratch;

    for (int q = 0; q < num_quads; q++)
    {
        // stb_easy_font: 4 вершины на квад, j=0 — top-left, j=2 — bottom-right.
        const uint8_t * base = p + (size_t)q * TEXT_STB_VERTS_PER_QUAD * TEXT_STB_VERTEX_BYTES;

        float fx0 = *(const float *)(base + 0 * TEXT_STB_VERTEX_BYTES + 0);
        float fy0 = *(const float *)(base + 0 * TEXT_STB_VERTEX_BYTES + 4);
        float fx1 = *(const float *)(base + 2 * TEXT_STB_VERTEX_BYTES + 0);
        float fy1 = *(const float *)(base + 2 * TEXT_STB_VERTEX_BYTES + 4);

        float x0 = (float)x + fx0 * scale + 0.5f;
        float y0 = (float)y + fy0 * scale + 0.5f;
        float x1 = (float)x + fx1 * scale + 0.5f;
        float y1 = (float)y + fy1 * scale + 0.5f;

        emit_quad_ndc(x0, y0, x1, y1, r, g, b, 1.0f);
    }
}


void overlay_rect(int x0, int y0, int x1, int y1, float r, float g, float b, float a)
{
    emit_quad_ndc((float)x0, (float)y0, (float)x1, (float)y1, r, g, b, a);
}

void overlay_rect_outline(int x0, int y0, int x1, int y1, float r, float g, float b, float a, int thickness)
{
    if (thickness < 1)
    {
        thickness = 1;
    }

    emit_quad_ndc(x0, y0, x1, y0 + thickness, r, g, b, a);
    emit_quad_ndc(x0, y1 - thickness, x1, y1, r, g, b, a);
    emit_quad_ndc(x0, y0, x0 + thickness, y1, r, g, b, a);
    emit_quad_ndc(x1 - thickness, y0, x1, y1, r, g, b, a);
}

int text_measure(const char * text, float scale)
{
    if (!text || !*text || scale <= 0.0f)
    {
        return 0;
    }

    int w = stb_easy_font_width((char *)text);
    return (int)((float)w * scale + 0.5f);
}

void text_end(void)
{
    if (g_vert_count == 0 || !g_program || !g_vao)
    {
        return;
    }

    // Полнооконный viewport — текст рисуется поверх всего.
    glViewport(0, 0, g_win_w, g_win_h);

    glUseProgram(g_program);

    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)g_vert_count * sizeof(text_vertex_t)), g_verts, GL_DYNAMIC_DRAW);

    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, g_vert_count);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
