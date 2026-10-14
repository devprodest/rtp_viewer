#include "hotkeys.h"

#include "commands.h"

static bool s_pressed     = false;
static bool r_pressed     = false;
static bool w_pressed     = false;
static bool one_pressed   = false;
static bool f1_pressed    = false;
static bool h_pressed     = false;
static bool h_was_down    = false;
static bool space_pressed = false;
static bool e_pressed     = false;
static bool c_pressed     = false;

void hotkeys_process(GLFWwindow * window)
{

    bool c     = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    bool s     = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool r     = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    bool w     = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool one   = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool f1    = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    bool h     = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool e     = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    bool ctrl  = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                 glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    if (e && !e_pressed)
    {
        cmd_toggle_saturation();
    }
    if (one && !one_pressed)
    {
        cmd_toggle_smoothing();
    }
    if (s && !s_pressed)
    {
        cmd_request_screenshot();
    }
    if (r && !r_pressed)
    {
        cmd_toggle_recording();
    }
    if (w && !w_pressed)
    {
        cmd_flush_saver();
    }
    if (f1 && !f1_pressed)
    {
        cmd_show_help();
    }
    if (space && !space_pressed)
    {
        cmd_toggle_pause();
    }

    if (h && ctrl && !h_was_down)
    {
        cmd_toggle_histogram_scale();
    }
    else if (h && !ctrl && !h_pressed)
    {
        cmd_toggle_histogram();
    }

    if (c && !c_pressed)
    {
        cmd_toggle_histogram_cdf();
    }

    c_pressed     = c;
    e_pressed     = e;
    s_pressed     = s;
    r_pressed     = r;
    w_pressed     = w;
    one_pressed   = one;
    f1_pressed    = f1;
    h_pressed     = h && !ctrl;
    h_was_down    = h;
    space_pressed = space;
}
