#include "gl_utils.h"
#include "log/logger.h"

#include <stdio.h>

/// \brief Компилирует один шейдер; при ошибке печатает лог и возвращает 0.
GLuint gl_utils_compile_shader(const char * source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info[ 512 ];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        log_error("Shader compilation error: %s\n", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

/// \brief Собирает программу из встроенного vertex-шейдера и указанного fragment.
GLuint gl_utils_build_program(const char * fragment_source)
{
    static const char * vertex_source =
        "#version 330 core\n"
        "layout(location=0) in vec2 aPos;\n"
        "layout(location=1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "}\n";

    GLuint vertex = gl_utils_compile_shader(vertex_source, GL_VERTEX_SHADER);
    if (!vertex)
    {
        return 0;
    }

    GLuint fragment = gl_utils_compile_shader(fragment_source, GL_FRAGMENT_SHADER);
    if (!fragment)
    {
        glDeleteShader(vertex);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertex);
    glAttachShader(prog, fragment);
    glLinkProgram(prog);

    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info[ 512 ];
        glGetProgramInfoLog(prog, sizeof(info), nullptr, info);
        log_error("Program linking error: %s\n", info);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(prog);
        return 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return prog;
}

/// \brief Создаёт VAO с полноэкранным квадом (pos + texcoord).
GLuint gl_utils_create_fullscreen_quad(void)
{
    static const float vertices[] = {
        -1.0f, 1.0f,  0.0f, 0.0f, //
        -1.0f, -1.0f, 0.0f, 1.0f, //
        1.0f,  -1.0f, 1.0f, 1.0f, //
        -1.0f, 1.0f,  0.0f, 0.0f, //
        1.0f,  -1.0f, 1.0f, 1.0f, //
        1.0f,  1.0f,  1.0f, 0.0f, //
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return vao;
}
