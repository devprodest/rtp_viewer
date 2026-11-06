#include "hotkeys.h"
#include "commands.h"

#include "render/ui/context_menu.h"

static bool f1_pressed    = false;
static bool f11_pressed   = false;
static bool key_0_pressed = false;
static bool key_1_pressed = false;
static bool key_2_pressed = false;
static bool space_pressed = false;
static bool esc_pressed   = false;
static bool left_pressed  = false;
static bool right_pressed = false;
static bool eq_pressed    = false;
static bool minus_pressed = false;
static bool b_pressed     = false;
static bool e_pressed     = false;
static bool f_pressed     = false;
static bool h_pressed     = false;
static bool h_was_down    = false;
static bool i_pressed     = false;
static bool l_pressed     = false;
static bool m_pressed     = false;
static bool r_pressed     = false;
static bool s_pressed     = false;
static bool t_pressed     = false;
static bool v_pressed     = false;
static bool w_pressed     = false;

void hotkeys_process(GLFWwindow * window)
{
    bool f1    = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    bool f11   = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
    bool esc   = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    bool key_0 = glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS;
    bool key_1 = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    bool key_2 = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    bool left  = glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS;
    bool right = glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS;
    bool eq    = glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS;
    bool minus = glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS;

    bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool r = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool h = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
    bool e = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
    bool i = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
    bool l = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    bool m = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    bool f = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    bool t = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    bool v = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
    bool b = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;

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
    if (key_0 && no_modificator && !key_0_pressed)
    {
        cmd_execute(CMD_ZOOM_RESET);
    }
    if (key_1 && no_modificator && !key_1_pressed)
    {
        cmd_execute(CMD_TOGGLE_SMOOTHING);
    }
    if (key_2 && no_modificator && !key_2_pressed)
    {
        cmd_execute(CMD_TOGGLE_AUTO_LEVELS);
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
    if (f11 && no_modificator && !f11_pressed)
    {
        cmd_execute(CMD_TOGGLE_FULLSCREEN);
    }
    if (space && no_modificator && !space_pressed)
    {
        cmd_execute(CMD_TOGGLE_PAUSE);
    }
    if (left && no_modificator && !left_pressed)
    {
        cmd_execute(CMD_STEP_BACKWARD);
    }
    if (right && no_modificator && !right_pressed)
    {
        cmd_execute(CMD_STEP_FORWARD);
    }
    if (i && no_modificator && !i_pressed)
    {
        cmd_execute(CMD_TOGGLE_INSPECTOR);
    }
    if (l && no_modificator && !l_pressed)
    {
        cmd_execute(CMD_TOGGLE_LINE_PROFILE);
    }
    if (f && no_modificator && !f_pressed)
    {
        cmd_execute(CMD_TOGGLE_GRADIENT);
    }
    if (t && no_modificator && !t_pressed)
    {
        cmd_execute(CMD_TOGGLE_STATS);
    }
    if (m && no_modificator && !m_pressed)
    {
        cmd_execute(CMD_TOGGLE_VECTORSCOPE);
    }

    if (h && ctrl && !shift && !alt && !h_was_down)
    {
        cmd_execute(CMD_TOGGLE_HISTOGRAM_SCALE);
    }
    else if (h && no_modificator && !h_pressed)
    {
        cmd_execute(CMD_TOGGLE_HISTOGRAM);
    }
    if (v && no_modificator && !v_pressed)
    {
        cmd_execute(CMD_SET_REFERENCE);
    }
    if (b && no_modificator && !b_pressed)
    {
        cmd_execute(CMD_TOGGLE_DIFF);
    }
    if (eq && no_modificator && !eq_pressed)
    {
        cmd_execute(CMD_DIFF_GAIN_UP);
    }
    if (minus && no_modificator && !minus_pressed)
    {
        cmd_execute(CMD_DIFF_GAIN_DOWN);
    }

    if (esc && no_modificator && !esc_pressed)
    {
        if (context_menu_is_open())
        {
            context_menu_close();
        }
        else
        {
            cmd_execute(CMD_CLEAR_SELECTION);
        }
    }

    key_0_pressed = key_0;
    key_1_pressed = key_1;
    key_2_pressed = key_2;
    f1_pressed    = f1;
    f11_pressed   = f11;
    space_pressed = space;
    left_pressed  = left;
    right_pressed = right;
    esc_pressed   = esc;
    minus_pressed = minus;
    eq_pressed    = eq;
    f_pressed     = f;
    i_pressed     = i;
    l_pressed     = l;
    m_pressed     = m;
    e_pressed     = e;
    s_pressed     = s;
    r_pressed     = r;
    w_pressed     = w;
    t_pressed     = t;
    v_pressed     = v;
    b_pressed     = b;
    h_pressed     = h && !ctrl;
    h_was_down    = h;
}
