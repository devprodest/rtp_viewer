#include "window.h"

#include <stddef.h>
#include <stdio.h>

static GLFWwindow * g_window = nullptr;
static bool g_is_fullscreen  = false;

static int g_windowed_x = 0;
static int g_windowed_y = 0;
static int g_windowed_w = 0;
static int g_windowed_h = 0;

static double g_last_click_time = 0.0;

/// \brief Callback двойного клика — переключает полноэкранный режим.
static void mouse_button_callback(GLFWwindow *, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
    {
        return;
    }

    double now = glfwGetTime();
    if (now - g_last_click_time < 0.4)
    {
        window_toggle_fullscreen();
        g_last_click_time = 0.0;
    }
    else
    {
        g_last_click_time = now;
    }
}

/// \brief Создаёт окно GLFW и GL-контекст.
bool window_init(int width, int height, const char * title)
{
    if (!glfwInit())
    {
        fprintf(stderr, "GLFW init failed\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    g_window = glfwCreateWindow(width, height, title ? title : "RTP Viewer", nullptr, nullptr);
    if (!g_window)
    {
        fprintf(stderr, "GLFW window creation failed\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);
    glfwSetMouseButtonCallback(g_window, mouse_button_callback);

    return true;
}

void window_cleanup(void)
{
    if (g_window)
    {
        glfwDestroyWindow(g_window);
        g_window = nullptr;
    }
    glfwTerminate();

    g_is_fullscreen = false;
}

GLFWwindow * window_get(void)
{
    return g_window;
}

bool window_should_close(void)
{
    return g_window ? (glfwWindowShouldClose(g_window) != 0) : true;
}

void window_poll_events(void)
{
    glfwPollEvents();
}

void window_swap_buffers(void)
{
    if (g_window)
    {
        glfwSwapBuffers(g_window);
    }
}

void window_get_framebuffer_size(int * w, int * h)
{
    if (g_window)
    {
        glfwGetFramebufferSize(g_window, w, h);
    }
    else
    {
        if (w)
        {
            *w = 0;
        }
        if (h)
        {
            *h = 0;
        }
    }
}

void window_set_title(const char * title)
{
    if (g_window)
    {
        glfwSetWindowTitle(g_window, title ? title : "");
    }
}

bool window_is_fullscreen(void)
{
    return g_is_fullscreen;
}

void window_toggle_fullscreen(void)
{
    if (!g_window)
    {
        return;
    }

    if (!g_is_fullscreen)
    {
        glfwGetWindowPos(g_window, &g_windowed_x, &g_windowed_y);
        glfwGetWindowSize(g_window, &g_windowed_w, &g_windowed_h);

        GLFWmonitor * target = nullptr;

        int win_center_x = g_windowed_x + (g_windowed_w / 2);
        int win_center_y = g_windowed_y + (g_windowed_h / 2);

        int monitor_count       = 0;
        GLFWmonitor ** monitors = glfwGetMonitors(&monitor_count);

        for (int i = 0; i < monitor_count; i++)
        {
            int mx = 0, my = 0;
            glfwGetMonitorPos(monitors[ i ], &mx, &my);

            const GLFWvidmode * mode = glfwGetVideoMode(monitors[ i ]);
            if (!mode)
            {
                continue;
            }

            int m_left   = mx;
            int m_top    = my;
            int m_right  = mx + mode->width;
            int m_bottom = my + mode->height;

            if (win_center_x >= m_left && win_center_x < m_right && win_center_y >= m_top && win_center_y < m_bottom)
            {
                target = monitors[ i ];
                break;
            }
        }

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
        glfwSetWindowMonitor(g_window, nullptr, g_windowed_x, g_windowed_y, g_windowed_w, g_windowed_h, 0);
        g_is_fullscreen = false;
    }
}
