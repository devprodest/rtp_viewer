#include "logger.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static FILE * g_file           = nullptr;
static log_level_e g_min_level = LOG_INFO;

static const char * level_tag(log_level_e level)
{
    switch (level)
    {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARN: return "WARN";
        case LOG_ERROR: return "ERROR";
        default: return "?";
    }
}

static void log_write(log_level_e level, const char * fmt, va_list args)
{
    char stamp[ 32 ];
    time_t t           = time(nullptr);
    struct tm * tm_now = localtime(&t);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm_now);

    if (level >= g_min_level)
    {
        fprintf(stderr, "[%s] %-5s ", stamp, level_tag(level));
        vfprintf(stderr, fmt, args);
        fputc('\n', stderr);
    }

    if (g_file)
    {
        fprintf(g_file, "[%s] %-5s ", stamp, level_tag(level));
        vfprintf(g_file, fmt, args);
        fputc('\n', g_file);
        fflush(g_file);
    }
}

bool logger_init(const char * logfile, log_level_e min_level)
{
    g_min_level = min_level;

    if (logfile && *logfile)
    {
        g_file = fopen(logfile, "a");
        if (!g_file)
        {
            fprintf(stderr, "logger: cannot open logfile '%s': %s\n", logfile, strerror(errno));
            return false;
        }
    }
    return true;
}

void logger_cleanup(void)
{
    if (g_file)
    {
        fclose(g_file);
        g_file = nullptr;
    }
}

void log_debug(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_DEBUG, fmt, args);
    va_end(args);
}

void log_info(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_INFO, fmt, args);
    va_end(args);
}

void log_warn(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_WARN, fmt, args);
    va_end(args);
}

void log_error(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write(LOG_ERROR, fmt, args);
    va_end(args);
}
