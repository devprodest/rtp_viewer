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
    bool f1    = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    bool one   = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool c     = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
    bool s     = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool r     = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    bool w     = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool h     = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    bool e     = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

    bool ctrl  = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || //
                 glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || //
                 glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    bool alt   = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || //
                 glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;

    bool no_modificator = !ctrl && !shift && !alt;

    if (e && no_modificator && !e_pressed)
    {
        cmd_execute(CMD_TOGGLE_SATURATION);
    }
    if (one && no_modificator && !one_pressed)
    {
        cmd_execute(CMD_TOGGLE_SMOOTHING);
    }
    if (s && no_modificator && !s_pressed)
    {
        cmd_execute(CMD_REQUEST_SCREENSHOT);
    }
    if (r && no_modificator && !r_pressed)
    {
        cmd_execute(CMD_TOGGLE_RECORDING);
    }
    if (w && no_modificator && !w_pressed)
    {
        cmd_execute(CMD_FLUSH_SAVER);
    }
    if (f1 && no_modificator && !f1_pressed)
    {
        cmd_execute(CMD_SHOW_HELP);
    }
    if (space && no_modificator && !space_pressed)
    {
        cmd_execute(CMD_TOGGLE_PAUSE);
    }

    if (h && ctrl && !shift && !alt && !h_was_down)
    {
        cmd_execute(CMD_TOGGLE_HISTOGRAM_SCALE);
    }
    else if (h && no_modificator && !h_pressed)
    {
        cmd_execute(CMD_TOGGLE_HISTOGRAM);
    }

    if (c && no_modificator && !c_pressed)
    {
        cmd_execute(CMD_TOGGLE_HISTOGRAM_CDF);
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
