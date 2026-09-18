// gl_render.c - реализация OpenGL, загрузка формата из .fmt

#include "gl_render.h"
#include "saver.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLFWwindow * g_window          = NULL;
static GLuint g_texture               = 0;
static GLuint g_program               = 0;
static GLint g_uniform_tex            = -1;
static GLint g_uniform_width          = -1;
static GLint g_uniform_height         = -1;
static GLint g_uniform_effective_bits = -1;
static GLint g_uniform_demosaic_mode  = -1;
static int g_width                    = 0;
static int g_height                   = 0;
static int g_eff_bits                 = 16;
static bool g_smoothing               = false;
static int g_demosaic_mode            = 0;

// Для переключения fullscreen двойным кликом
static bool g_is_fullscreen     = false;
static int g_windowed_x         = 0;
static int g_windowed_y         = 0;
static int g_windowed_w         = 0;
static int g_windowed_h         = 0;
static double g_last_click_time = 0.0;

// Для заголовка окна
static char g_format_name[64] = {0};

typedef struct format_config_s
{
    char name[64];
    int bytes_per_pixel;
    int gl_internal_format;
    int gl_format;
    int gl_type;
    char shader_file[256];
    bool needs_demosaic;
    char bayer_pattern[8];
} format_config_s;

static format_config_s g_format;

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

static bool parse_format_file(const char * format_name, format_config_s * cfg)
{
    char path[256];
    snprintf(path, sizeof(path), "formats/%s.fmt", format_name);

    FILE * f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "Cannot open format file: %s\n", path);
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

    char line[256];
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
            cfg->bytes_per_pixel = atoi(value);
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
                fprintf(stderr, "Unknown internal format: %s\n", value);
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
                fprintf(stderr, "Unknown format: %s\n", value);
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
                fprintf(stderr, "Unknown type: %s\n", value);
            }
        }
        else if (strcmp(key, "shader_file") == 0)
        {
            strncpy(cfg->shader_file, value, sizeof(cfg->shader_file) - 1);
            cfg->shader_file[sizeof(cfg->shader_file) - 1] = '\0';
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

static GLuint compile_shader(const char * source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info[512];
        glGetShaderInfoLog(shader, sizeof(info), NULL, info);
        fprintf(stderr, "Shader compilation error: %s\n", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint build_program(const char * fragment_source)
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

    GLuint vertex = compile_shader(vertex_source, GL_VERTEX_SHADER);
    if (!vertex)
    {
        return 0;
    }

    GLuint fragment = compile_shader(fragment_source, GL_FRAGMENT_SHADER);
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
        char info[512];
        glGetProgramInfoLog(prog, sizeof(info), NULL, info);
        fprintf(stderr, "Program linking error: %s\n", info);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(prog);
        return 0;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return prog;
}

static GLuint g_vao = 0;

static void create_fullscreen_quad(GLuint * out_vao)
{
    static const float vertices[] = {
        -1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  -1.0f, 1.0f, 1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    *out_vao = vao;
}

// Формируем заголовок: "RTP Viewer - WxH format, smoothing: on/off [REC]"
void gl_render_refresh_title(void)
{
    if (!g_window)
    {
        return;
    }

    char title[160];

    snprintf(title, sizeof(title), "RTP Viewer - %dx%d %s%s%s", g_width, g_height, //
             g_format_name[0] ? g_format_name : "?",                               //
             g_smoothing ? " [SMOOTH]" : "",                                       //
             saver_is_recording() ? " [REC]" : "");

    glfwSetWindowTitle(g_window, title);
}

static void mouse_button_callback(GLFWwindow * window, int button, int action, int mods)
{
    (void)mods;

    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
    {
        return;
    }

    double now = glfwGetTime();

    // Двойной клик: второй клик в течение 400 мс после первого
    if (now - g_last_click_time < 0.4)
    {
        gl_render_toggle_fullscreen();
        g_last_click_time = 0.0; // сброс, чтобы тройной клик не сработал дважды
    }
    else
    {
        g_last_click_time = now;
    }
}

void gl_render_toggle_fullscreen(void)
{
    if (!g_window)
    {
        return;
    }

    if (!g_is_fullscreen)
    {
        // Сохраняем текущую позицию и размер окна
        glfwGetWindowPos(g_window, &g_windowed_x, &g_windowed_y);
        glfwGetWindowSize(g_window, &g_windowed_w, &g_windowed_h);

        // Находим монитор, на котором сейчас находится окно
        GLFWmonitor * target = NULL;

        int win_center_x = g_windowed_x + g_windowed_w / 2;
        int win_center_y = g_windowed_y + g_windowed_h / 2;

        int monitor_count       = 0;
        GLFWmonitor ** monitors = glfwGetMonitors(&monitor_count);

        for (int i = 0; i < monitor_count; i++)
        {
            int mx = 0, my = 0;
            glfwGetMonitorPos(monitors[i], &mx, &my);

            const GLFWvidmode * mode = glfwGetVideoMode(monitors[i]);
            if (!mode)
            {
                continue;
            }

            // Границы монитора в виртуальных координатах
            int m_left   = mx;
            int m_top    = my;
            int m_right  = mx + mode->width;
            int m_bottom = my + mode->height;

            if (win_center_x >= m_left && win_center_x < m_right && win_center_y >= m_top && win_center_y < m_bottom)
            {
                target = monitors[i];
                break;
            }
        }

        // Если по каким-то причинам не нашли — берём основной
        if (!target)
        {
            target = glfwGetPrimaryMonitor();
        }

        if (!target)
        {
            return;
        }

        const GLFWvidmode * mode = glfwGetVideoMode(target);
        if (!mode)
        {
            return;
        }

        glfwSetWindowMonitor(g_window, target, 0, 0, mode->width, mode->height, mode->refreshRate);

        g_is_fullscreen = true;
    }
    else
    {
        // Возврат в оконный режим
        glfwSetWindowMonitor(g_window, NULL, g_windowed_x, g_windowed_y, g_windowed_w, g_windowed_h, 0);

        g_is_fullscreen = false;
    }
}

bool gl_render_init(int width, int height, const char * format_name, int effective_bits, bool smooth_start)
{
    if (!glfwInit())
    {
        fprintf(stderr, "GLFW init failed\n");
        return false;
    }

    if (!parse_format_file(format_name, &g_format))
    {
        glfwTerminate();
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    g_window = glfwCreateWindow(width, height, "RTP Viewer", NULL, NULL);
    if (!g_window)
    {
        fprintf(stderr, "GLFW window creation failed\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    glfwSetMouseButtonCallback(g_window, mouse_button_callback);

    if (gl_api_load() != 0)
    {
        fprintf(stderr, "Failed to load OpenGL functions\n");
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    char shader_path[512];
    snprintf(shader_path, sizeof(shader_path), "formats/%s", g_format.shader_file);

    char * frag_source = read_file(shader_path);
    if (!frag_source)
    {
        fprintf(stderr, "Failed to load shader file: %s\n", shader_path);
        glfwDestroyWindow(g_window);
        glfwTerminate();
        return false;
    }

    g_program = build_program(frag_source);
    free(frag_source);
    if (!g_program)
    {
        glfwDestroyWindow(g_window);
        glfwTerminate();
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
        glUniform1i(g_uniform_effective_bits, effective_bits);
    }
    if (g_uniform_demosaic_mode >= 0)
    {
        glUniform1i(g_uniform_demosaic_mode, 0);
    }

    g_width    = width;
    g_height   = height;
    g_eff_bits = effective_bits;

    snprintf(g_format_name, sizeof(g_format_name), "%s", format_name);

    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, g_format.gl_internal_format, width, height, 0, g_format.gl_format, g_format.gl_type,
                 NULL);

    // Начальное состояние фильтра: toggle инвертирует флаг
    g_smoothing = !smooth_start;
    gl_render_toggle_smoothing();

    create_fullscreen_quad(&g_vao);

    return true;
}

GLFWwindow * gl_render_get_window(void)
{
    return g_window;
}

void gl_render_update_texture(const uint8_t * data)
{
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_width, g_height, g_format.gl_format, g_format.gl_type, data);

    int win_w = 0;
    int win_h = 0;
    glfwGetFramebufferSize(g_window, &win_w, &win_h);

    float aspect_img = (float)g_width / (float)g_height;
    float aspect_win = (float)win_w / (float)win_h;

    int vp_w = 0;
    int vp_h = 0;
    int vp_x = 0;
    int vp_y = 0;

    if (aspect_win > aspect_img)
    {
        vp_h = win_h;
        vp_w = (int)(win_h * aspect_img);
        vp_x = (win_w - vp_w) / 2;
    }
    else
    {
        vp_w = win_w;
        vp_h = (int)(win_w / aspect_img);
        vp_y = (win_h - vp_h) / 2;
    }

    glViewport(vp_x, vp_y, vp_w, vp_h);
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

    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glfwSwapBuffers(g_window);
}

void gl_render_poll_events(void)
{
    glfwPollEvents();
}

void gl_render_cleanup(void)
{
    if (g_vao)
    {
        glDeleteVertexArrays(1, &g_vao);
    }
    if (g_program)
    {
        glDeleteProgram(g_program);
    }
    if (g_texture)
    {
        glDeleteTextures(1, &g_texture);
    }
    if (g_window)
    {
        glfwDestroyWindow(g_window);
    }
    glfwTerminate();

    g_vao     = 0;
    g_program = 0;
    g_texture = 0;
    g_window  = NULL;
}

void gl_render_toggle_smoothing(void)
{
    g_smoothing = !g_smoothing;
    glBindTexture(GL_TEXTURE_2D, g_texture);
    if (g_smoothing)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    gl_render_refresh_title();

    printf("Smoothing %s\n", g_smoothing ? "enabled" : "disabled");
}

void gl_render_set_demosaic_mode(int mode)
{
    g_demosaic_mode = mode;
}

int gl_render_get_bytes_per_pixel(void)
{
    return g_format.bytes_per_pixel;
}
