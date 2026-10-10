#pragma once

#include <stdbool.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

/// \file mouse_state.h
/// \brief Состояние мыши: позиция, кнопки, накопленный wheel.
/// \date 2026
///
/// \details
/// Позиция и кнопки читаются через polling (glfwGetCursorPos /
/// glfwGetMouseButton), как и клавиши в process_input. Wheel — только
/// через callback, потому что события колеса не имеют состояния
/// «нажато/не нажато» и polling для них невозможен.
///
/// Координаты курсора — в пикселях окна, от левого верхнего угла.
/// Это «сырые» оконные координаты; перевод в координаты кадра делает
/// gl_render (см. screen_to_frame).

/// \brief Снимок состояния мыши на текущий кадр.
typedef struct mouse_state_s
{
    double x;           ///< Позиция по X в пикселях окна
    double y;           ///< Позиция по Y в пикселях окна
    bool left;          ///< ЛКМ нажата
    bool middle;        ///< СКМ нажата
    bool right;         ///< ПКМ нажата
    double wheel_accum; ///< Накопленный wheel с последнего take
} mouse_state_s;

void mouse_state_init(GLFWwindow * window);
void mouse_state_update(GLFWwindow * window, mouse_state_s * m);
double mouse_state_take_wheel(mouse_state_s * m);
