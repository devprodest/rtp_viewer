#pragma once

#include <stdbool.h>

typedef enum log_level_e
{
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
} log_level_e;

/// \brief Инициализирует логгер.
///
/// \param logfile Путь к файлу для логов или nullptr (только консоль).
/// \param min_level Минимальный уровень для вывода в консоль.
bool logger_init(const char * logfile, log_level_e min_level);

/// \brief Закрывает логгер.
void logger_cleanup(void);

void log_debug(const char * fmt, ...);
void log_info(const char * fmt, ...);
void log_warn(const char * fmt, ...);
void log_error(const char * fmt, ...);