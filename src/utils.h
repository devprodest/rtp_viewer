// utils.h - вспомогательные функции

#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

// Получить текущее время в микросекундах
uint64_t get_time_us(void);

// Получить текущее время в секундах
uint64_t get_time_s(void);

// Прочитать весь файл в выделенную память (NULL при ошибке)
char* read_file(const char *path);

#endif
