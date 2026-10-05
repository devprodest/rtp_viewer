#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return ((uint64_t)tv.tv_sec * 1000000ull) + (uint64_t)tv.tv_usec;
}

uint64_t get_time_s(void)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint64_t)tv.tv_sec;
}

char * read_file(const char * path)
{
    FILE * f = fopen(path, "rb");
    if (!f)
    {
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char * buf = malloc((size_t)len + 1);
    if (!buf)
    {
        fclose(f);
        return nullptr;
    }

    size_t got = fread(buf, 1, (size_t)len, f);
    buf[got]   = '\0';
    fclose(f);
    return buf;
}

// Строгий парсинг int. 0 - успех, -1 - ошибка.
int parse_int(const char * s, long min, long max, long * out)
{
    if (!s || !*s)
    {
        return -1;
    }

    errno      = 0;
    char * end = nullptr;
    long v     = strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0')
    {
        return -1;
    }
    if (v < min || v > max)
    {
        return -1;
    }

    *out = v;
    return 0;
}
