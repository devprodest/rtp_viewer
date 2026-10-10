#include "mouse_state.h"

#include <stddef.h>

static double g_wheel_pending = 0.0;

static void scroll_callback(GLFWwindow * window, double xoff, double yoff)
{
    (void)window;
    (void)xoff;
    g_wheel_pending += yoff;
}

/// \brief Инициализирует подсистему мыши.
///
/// \details Регистрирует scroll callback на окне. Позиция и кнопки
///          читаются в mouse_state_update через polling, callback не нужен.
///
/// \param window Окно GLFW
void mouse_state_init(GLFWwindow * window)
{
    if (!window)
    {
        return;
    }

    g_wheel_pending = 0.0;
    glfwSetScrollCallback(window, scroll_callback);
}

/// \brief Обновляет снимок состояния.
///
/// \details Вызывать раз в кадр из главного цикла, до process_input.
///
/// \param window Окно GLFW
/// \param m      Снимок, который нужно обновить
void mouse_state_update(GLFWwindow * window, mouse_state_s * m)
{
    if (!window || !m)
    {
        return;
    }

    glfwGetCursorPos(window, &m->x, &m->y);

    m->left   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    m->middle = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    m->right  = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

/// \brief Забирает накопленный wheel и обнуляет счётчик.
///
/// \param m Снимок
///
/// \return Накопленное значение wheel (положительное — вверх/от себя)
double mouse_state_take_wheel(mouse_state_s * m)
{
    double w        = g_wheel_pending;
    g_wheel_pending = 0.0;
    if (m)
    {
        m->wheel_accum = 0.0;
    }
    return w;
}
