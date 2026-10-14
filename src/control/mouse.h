#pragma once

#include <stdbool.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

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
