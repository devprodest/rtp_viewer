#pragma once

/// \file text_overlay.h
/// \brief Отрисовка ASCII-текста поверх GL-сцены.
/// \date 2026
///
/// \details
/// Стабильный интерфейс независимо от реализации. Внутренний рендерер
/// можно заменить (stb_easy_font → bitmap-атлас → что угодно) без правок
/// у вызывающего кода.
///
/// Типичный цикл:
///   text_begin(win_w, win_h);
///   text_draw(10, 10, "Hello", 1.0f, 1.0f, 1.0f, 1.0f);
///   text_draw(10, 30, "World", 1.0f, 0.0f, 0.0f, 1.0f);
///   text_end();

/// \brief Инициализирует рендерер текста.
///
/// \warning Вызывать после glfwMakeContextCurrent и gl_api_load.
void text_overlay_init(void);

/// \brief Освобождает ресурсы.
void text_overlay_cleanup(void);

/// \brief Начинает кадр отрисовки текста.
///
/// \details Задаёт размеры окна для перевода координат. Сбрасывает
///          накопленный буфер вершин.
///
/// \param win_w Ширина окна в пикселях
/// \param win_h Высота окна в пикселях
void text_begin(int win_w, int win_h);

/// \brief Добавляет строку в буфер кадра.
///
/// \param x     Позиция по X от левого верхнего угла окна, пиксели
/// \param y     Позиция по Y от левого верхнего угла окна, пиксели
/// \param text  Строка ASCII (заканчивается NUL)
/// \param r     Красный 0..1
/// \param g     Зелёный 0..1
/// \param b     Синий 0..1
/// \param scale Множитель размера (1.0 = базовый, обычно ~12 px)
void text_draw(int x, int y, const char * text, float r, float g, float b, float scale);

/// \brief Возвращает ширину строки в пикселях.
///
/// \param text  Строка
/// \param scale Множитель размера
///
/// \return Ширина в пикселях
int text_measure(const char * text, float scale);

/// \brief Рисует всё накопленное одним draw call.
void text_end(void);
