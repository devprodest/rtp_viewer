#pragma once

#include <stddef.h>
#include <stdint.h>

uint64_t get_time_us(void);
uint64_t get_time_s(void);
char * read_file(const char * path);

int parse_int(const char * s, long min, long max, long * out);
