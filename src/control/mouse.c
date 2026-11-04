#include "mouse.h"

static double g_wheel_pending = 0.0;

static void scroll_callback(GLFWwindow * /*unused*/, double /*unused*/, double yoff)
{
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

    double px = 0.0;
    double py = 0.0;
    glfwGetCursorPos(window, &px, &py);

    // glfwGetCursorPos возвращает позицию в screen coords (window coords).
    // Renderer и viewport работают в framebuffer coords. Приводим по реальному отношению framebuffer/window, а не по
    // content_scale — он на некоторых конфигурациях врёт (framebuffer == window, но content_scale > 1), и тогда
    // координаты «уползают» вправо-вниз.
    int fb_w = 0;
    int fb_h = 0;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(window, &win_w, &win_h);

    const double sx = (win_w > 0) ? (double)fb_w / (double)win_w : 1.0;
    const double sy = (win_h > 0) ? (double)fb_h / (double)win_h : 1.0;

    m->x = px * sx;
    m->y = py * sy;

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
