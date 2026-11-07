#include "hotkeys.h"
#include "commands.h"

#include <stddef.h>

static void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods)
{
    (void)window;
    (void)scancode;

    if (action != GLFW_PRESS)
    {
        return;
    }

    const int mod_mask = GLFW_MOD_CONTROL | GLFW_MOD_SHIFT | GLFW_MOD_ALT;
    const int pressed  = mods & mod_mask;

    int count                   = 0;
    const command_info_s * cmds = cmd_get_all(&count);

    for (int i = 0; i < count; i++)
    {
        if (cmds[ i ].type == CMD_TYPE_NONE || cmds[ i ].key == 0)
        {
            continue;
        }

        if (cmds[ i ].key == key && cmds[ i ].mods_required == pressed)
        {
            cmd_execute((commands_e)i);
            return;
        }
    }
}

void hotkeys_init(GLFWwindow * window)
{
    glfwSetKeyCallback(window, key_callback);
}
