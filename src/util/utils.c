#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <limits.h>
    #include <unistd.h>
#endif

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
    buf[ got ] = '\0';
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

/// \brief Формирует полное имя файла по единому шаблону:
///        [<dir>/]<prefix>_<YYYYMMDD_HHMMSS>_<WxH>_<format>[_<suffix>].<ext>
///
/// Примеры:
///   utils_build_filename(..., ".", "rtp", t, {800,480}, "rgb888", "0001", "raw")
///     → "./rtp_20261106_181223_800x480_rgb888_0001.raw"
///   utils_build_filename(..., ".", "rtp", t, {800,480}, "rgb888", "screenshot", "png")
///     → "./rtp_20261106_181223_800x480_rgb888_screenshot.png"
///   utils_build_filename(..., ".", "rtp", t, {800,480}, "rgb888", "defects", "csv")
///     → "./rtp_20261106_181223_800x480_rgb888_defects.csv"
///
/// \param out       Выходной буфер (рекомендуется >= 256)
/// \param outsz     Размер буфера
/// \param dir       Директория; NULL или "" = без префикса директории
/// \param prefix    Префикс; NULL или "" = "rtp"
/// \param timestamp Время для штампа; 0 = текущее
/// \param image     Размер кадра
/// \param format    Имя формата; NULL или "" = "unknown"
/// \param suffix    Суффикс (например, "0001", "screenshot"); NULL/"" = без суффикса
/// \param ext       Расширение без точки (обязательно)
/// \return true при успехе, false если параметры некорректны или не влезло
bool utils_build_filename(char * out, size_t outsz, const char * dir, const char * prefix, time_t timestamp,
                          image_size_s image, const char * format, const char * suffix, const char * ext)
{
    if (!out || outsz == 0 || !ext || !*ext)
    {
        return false;
    }

    if (timestamp == 0)
    {
        timestamp = time(nullptr);
    }
    if (!prefix || !*prefix)
    {
        prefix = "rtp";
    }
    if (!format || !*format)
    {
        format = "unknown";
    }

    struct tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &timestamp);
#else
    localtime_r(&timestamp, &tm_buf);
#endif

    char stamp[ 32 ];
    if (strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm_buf) == 0)
    {
        return false;
    }

    // Базовая часть: <prefix>_<stamp>_<WxH>_<format>
    char base[ 384 ];
    int bn = snprintf(base, sizeof(base), "%s_%s_%dx%d_%s", prefix, stamp, image.x, image.y, format);
    if (bn <= 0 || (size_t)bn >= sizeof(base))
    {
        return false;
    }

    int n;
    if (dir && *dir)
    {
        if (suffix && *suffix)
        {
            n = snprintf(out, outsz, "%s/%s_%s.%s", dir, base, suffix, ext);
        }
        else
        {
            n = snprintf(out, outsz, "%s/%s.%s", dir, base, ext);
        }
    }
    else
    {
        if (suffix && *suffix)
        {
            n = snprintf(out, outsz, "%s_%s.%s", base, suffix, ext);
        }
        else
        {
            n = snprintf(out, outsz, "%s.%s", base, ext);
        }
    }

    return n > 0 && (size_t)n < outsz;
}

/// \brief Возвращает директорию исполняемого файла (без завершающего слэша).
///        На Windows — через GetModuleFileNameA, на Linux — через /proc/self/exe.
///        Если определить не удалось — возвращает "." (текущую директорию).
///
/// \return true при успехе, false если out слишком мал или путь не определён
bool utils_get_exe_dir(char * out, size_t outsz)
{
    if (!out || outsz < 2)
    {
        return false;
    }

#ifdef _WIN32
    char path[ MAX_PATH ];
    DWORD len = GetModuleFileNameA(NULL, path, sizeof(path));
    if (len == 0 || len >= sizeof(path))
    {
        snprintf(out, outsz, ".");
        return false;
    }
    // Обрезаем имя файла: ищем последний слэш или обратный слэш
    char * last_sep = strrchr(path, '\\');
    if (!last_sep)
    {
        last_sep = strrchr(path, '/');
    }
    if (!last_sep)
    {
        snprintf(out, outsz, ".");
        return false;
    }
    *last_sep = '\0';
    snprintf(out, outsz, "%s", path);
    return true;
#else
    char path[ PATH_MAX ];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0 || len >= (ssize_t)sizeof(path))
    {
        snprintf(out, outsz, ".");
        return false;
    }
    path[ len ]     = '\0';
    char * last_sep = strrchr(path, '/');
    if (!last_sep)
    {
        snprintf(out, outsz, ".");
        return false;
    }
    *last_sep = '\0';
    snprintf(out, outsz, "%s", path);
    return true;
#endif
}
