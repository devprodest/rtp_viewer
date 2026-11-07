/// \file saver.c
/// \brief Сохранение кадров в отдельном потоке с пулом буферов.
/// \date 2026
///
/// \details
/// Принимает кадры из главного потока и пишет их на диск в отдельном
/// потоке. Кадры ходят по маршруту pool → queue → saver thread → pool
/// без динамических аллокаций в горячем пути. Если saver-поток не успевает
/// за потоком кадров, новые кадры дропаются (main при этом не блокируется).
///
/// Пул — одна непрерывная аллокация размером pool_capacity × frame_size;
/// отдельные буферы — указатели внутрь блока. При нехватке памяти
/// capacity уменьшается вдвое, пока не дойдёт до SAVER_POOL_MIN.
///
/// Файлы ротации: по batch_size кадров или по time_window секунд.
/// Имя файла: <outdir>/<prefix>_<YYYYMMDD_HHMMSS>_<WxH>_<format>_<NNNN>.raw,
/// где NNNN — счётчик файлов, 4 цифры.
///
/// \warning Работает с диска; не использовать на очень медленных носителях
///          без уменьшения SAVER_POOL_MAX — иначе будут дропы.

//----------------------------------------------------- Includes -------------------------------------------------------
#include "saver.h"
#include "log/logger.h"
#include "util/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

//------------------------------------------------------ Macros --------------------------------------------------------

/// \brief Минимальная версия Win32 API для CONDITION_VARIABLE (Vista).
///
/// \details CONDITION_VARIABLE появилась в Vista. Без этого define mingw
///          не объявит SleepConditionVariableCS / InitializeConditionVariable.
///
/// Значение: 0x0600 (Vista). Менять не нужно.
#define SAVER_WIN32_WINNT          0x0600

/// \brief Максимальная длина префикса имени файла (с NUL).
///
/// \details Задаётся через -s; по умолчанию "rtp". Более длинные значения
///          обрезаются через strncpy.
///
/// Допустимый диапазон: 2..256. По умолчанию: 256.
#define SAVER_PREFIX_MAX           256

/// \brief Максимальная длина выходной директории (с NUL).
///
/// \details Задаётся через -o; по умолчанию ".". Более длинные пути
///          обрезаются.
///
/// Допустимый диапазон: 2..256. По умолчанию: 256.
#define SAVER_OUTDIR_MAX           256

/// \brief Максимальная длина имени формата (с NUL).
///
/// \details Берётся из -f. Формат — часть имени файла.
///
/// Допустимый диапазон: 2..64. По умолчанию: 64.
#define SAVER_FORMAT_MAX           64

/// \brief Максимальный размер полного пути файла кадра (с NUL).
///
/// \details Включает outdir + prefix + timestamp + WxH + format + счётчик.
///          Должен быть не меньше суммы остальных буферов + разделители.
///
/// Допустимый диапазон: >= SAVER_OUTDIR_MAX + SAVER_PREFIX_MAX + 64.
/// По умолчанию: 1024.
#define SAVER_NAME_MAX             1024

/// \brief Максимальное число буферов в пуле.
///
/// \details Верхняя граница: сколько буферов выделяется при saver_init,
///          если хватает памяти. Пиковая память = SAVER_POOL_MAX × frame_size,
///          выделяется одной аллокацией.
///          При нехватке памяти число буферов уменьшается вдвое (см. MIN).
///
/// Влияет на: буферизацию при записи; слишком маленькое значение — частые
/// дропы при медленном диске; слишком большое — много памяти.
///
/// Допустимый диапазон: 4..255. По умолчанию: 64.
#define SAVER_POOL_MAX             64

/// \brief Минимальное число буферов в пуле.
///
/// \details Нижняя граница уменьшения пула при нехватке памяти. Если и
///          столько буферов не удалось выделить — saver отключается целиком.
///
/// Допустимый диапазон: 2..SAVER_POOL_MAX. По умолчанию: 4.
/// Нельзя ставить меньше 2: очередь и пул рассчитаны на разделение
/// хотя бы одного буфера «in-flight» и одного «в очереди».
#define SAVER_POOL_MIN             4

/// \brief Префикс имени файла по умолчанию.
///
/// \details Используется, если -s не задан.
#define SAVER_DEFAULT_PREFIX       "rtp"

/// \brief Права доступа к создаваемой выходной директории (POSIX).
///
/// \details 0755 = rwxr-xr-x. На Windows mkdir(path) без режима.
#define SAVER_DIR_MODE_POSIX       0755

// Проверки на этапе компиляции: сужение до uint8_t корректно только
// при этих условиях.
_Static_assert(SAVER_POOL_MAX <= 255, "SAVER_POOL_MAX must fit in uint8_t");
_Static_assert(SAVER_POOL_MIN >= 2, "SAVER_POOL_MIN must be at least 2");
_Static_assert(SAVER_POOL_MIN <= SAVER_POOL_MAX, "SAVER_POOL_MIN must be <= SAVER_POOL_MAX");

//----------------------------------------------------------------------------------------------------------------------
// Абстракция потоков/mutex/condvar — Win32 либо pthreads.
//----------------------------------------------------------------------------------------------------------------------

#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT SAVER_WIN32_WINNT
    #endif
    #include <windows.h>

/// \brief Мьютекс (Win32 — CRITICAL_SECTION).
typedef CRITICAL_SECTION saver_mutex_t;

/// \brief Условная переменная (Win32 — CONDITION_VARIABLE).
typedef CONDITION_VARIABLE saver_cond_t;

/// \brief Дескриптор потока (Win32 — HANDLE).
typedef HANDLE saver_thread_t;

    #define SAVER_MUTEX_INIT(m)    InitializeCriticalSection(m)
    #define SAVER_MUTEX_DESTROY(m) DeleteCriticalSection(m)
    #define SAVER_MUTEX_LOCK(m)    EnterCriticalSection(m)
    #define SAVER_MUTEX_UNLOCK(m)  LeaveCriticalSection(m)

    #define SAVER_COND_INIT(c)     InitializeConditionVariable(c)
    #define SAVER_COND_DESTROY(c)  ((void)0)
    #define SAVER_COND_WAIT(c, m)  SleepConditionVariableCS((c), (m), INFINITE)
    #define SAVER_COND_SIGNAL(c)   WakeConditionVariable(c)

#else
    #include <pthread.h>

/// \brief Мьютекс (POSIX).
typedef pthread_mutex_t saver_mutex_t;

/// \brief Условная переменная (POSIX).
typedef pthread_cond_t saver_cond_t;

/// \brief Идентификатор потока (POSIX).
typedef pthread_t saver_thread_t;

    #define SAVER_MUTEX_INIT(m)    pthread_mutex_init((m), nullptr)
    #define SAVER_MUTEX_DESTROY(m) pthread_mutex_destroy(m)
    #define SAVER_MUTEX_LOCK(m)    pthread_mutex_lock(m)
    #define SAVER_MUTEX_UNLOCK(m)  pthread_mutex_unlock(m)

    #define SAVER_COND_INIT(c)     pthread_cond_init((c), nullptr)
    #define SAVER_COND_DESTROY(c)  pthread_cond_destroy(c)
    #define SAVER_COND_WAIT(c, m)  pthread_cond_wait((c), (m))
    #define SAVER_COND_SIGNAL(c)   pthread_cond_signal(c)

#endif

//----------------------------------------------------- Typedefs -------------------------------------------------------

/// \brief Пул свободных буферов кадра.
///
/// \details Все буферы — срезы одной непрерывной аллокации block:
///          bufs[i] = block + i * frame_size. При capacity == 0 блок не
///          выделен, bufs не валиден.
///
/// \note Индексы сужены до uint8_t: SAVER_POOL_MAX == 64, влезает с запасом.
///       _Static_assert выше ловит случай, если MAX когда-нибудь превысит 255.
typedef struct saver_pool_s
{
    uint8_t * block;                  ///< Один непрерывный блок на capacity × frame_size
    uint8_t * bufs[ SAVER_POOL_MAX ]; ///< Указатели на срезы внутри block
    uint8_t capacity;                 ///< Фактическое число буферов (SAVER_POOL_MIN..MAX)
    uint8_t free_count;               ///< Текущее число свободных буферов (0..capacity)
} saver_pool_t;

/// \brief Кольцевая очередь занятых буферов, ожидающих записи.
///
/// \details Размер массива — SAVER_POOL_MAX, модуль кольца — pool.capacity,
///          а не SAVER_POOL_MAX. head/tail/count всегда 0..capacity.
typedef struct saver_queue_s
{
    uint8_t * bufs[ SAVER_POOL_MAX ]; ///< Буферы в очереди
    uint8_t head;                     ///< Индекс головы (что забрать следующим)
    uint8_t tail;                     ///< Индекс хвоста (куда положить следующий)
    uint8_t count;                    ///< Текущее число элементов
} saver_queue_t;

/// \brief Контекст сохранятеля (один глобальный экземпляр).
///
/// \details Порядок полей выбран так, чтобы:
///          1. Всё, что трогает saver_add_frame / saver_thread_proc,
///             лежало в первых ~64 байтах (одна кэш-линия).
///          2. Между полями не появлялось автоматических паддингов.
///          3. Большие массивы (pool, queue) — в конце, вне горячей зоны.
///
/// Секции помечены hot/warm/cold по частоте доступа.
typedef struct saver_context_s
{
    // ---- HOT: синхронизация. Трогается в каждом saver_add_frame ----
    saver_mutex_t mutex; ///< Защищает pool, queue, recording, flags
    saver_cond_t cond;   ///< Сигнал saver-потоку о новой работе

    // ---- HOT: флаги (упакованы без паддинга) ----
    volatile bool enabled; ///< Инициализирован ли saver (пул + поток)
    bool recording;        ///< Идёт ли запись (переключается R / -s)
    bool stop_requested;   ///< Запрос на завершение потока
    bool flush_requested;  ///< Запрос на закрытие текущего файла

    // ---- HOT: счётчики и индексы под mutex (uint8_t, 0..SAVER_POOL_MAX) ----
    uint8_t pool_capacity;   ///< Копия pool.capacity для быстрого modulo
    uint8_t pool_free_count; ///< Копия pool.free_count
    uint8_t queue_head;      ///< Копия queue.head
    uint8_t queue_tail;      ///< Копия queue.tail
    uint8_t queue_count;     ///< Копия queue.count
    uint8_t _pad0;           ///< Явный паддинг до 4-байтового выравнивания

    uint32_t file_counter; ///< Счётчик файлов, растёт на каждый новый
    uint32_t _pad1;        ///< Паддинг до 8-байтового выравнивания

    uint64_t dropped; ///< Сколько кадров дропнуто из-за пустого пула

    // ---- WARM: saver-поток, не трогается из main ----
    saver_thread_t thread;   ///< Дескриптор saver-потока
    FILE * current_file;     ///< Открытый файл или nullptr
    time_t batch_start_time; ///< Время открытия текущего файла
    uint32_t frames_in_file; ///< Кадров записано в текущий файл
    uint32_t _pad2;          ///< Паддинг до 8-байтового выравнивания

    // ---- COLD: config, только чтение после saver_init ----
    uint32_t frame_size;  ///< Размер кадра в байтах
    uint32_t batch_size;  ///< Кадров на файл (>= 1)
    uint32_t time_window; ///< Секунд на файл (0 = только по количеству)
    image_size_s image;
    char format_name[ SAVER_FORMAT_MAX ]; ///< Имя формата (для имени файла)

    // ---- COLD: строковые буферы ----
    char prefix[ SAVER_PREFIX_MAX ];         ///< Префикс имён файлов
    char outdir[ SAVER_OUTDIR_MAX ];         ///< Директория сохранения
    char current_filename[ SAVER_NAME_MAX ]; ///< Путь текущего файла

    // ---- BIG: пул и очередь в конце, вне горячей зоны ----
    saver_pool_t pool;   ///< Свободные буферы
    saver_queue_t queue; ///< Очередь на запись
} saver_context_s;

//---------------------------------------------------- Variables -------------------------------------------------------

static saver_context_s g_saver; ///< Единственный экземпляр контекста

//------------------------------------------------- Static Functions ---------------------------------------------------

/// \brief Проверяет, что задана нетривиальная выходная директория.
///
/// \return true, если outdir не пуст и не равен "."
static bool has_outdir(void)
{
    return g_saver.outdir[ 0 ] != '\0' && strcmp(g_saver.outdir, ".") != 0;
}

static void build_filename(char * out, size_t outsz)
{
    time_t t = g_saver.batch_start_time ? g_saver.batch_start_time : time(nullptr);

    char counter[ 16 ];
    snprintf(counter, sizeof(counter), "%04u", (unsigned)g_saver.file_counter);

    utils_build_filename(out,                 //
                         outsz,               //
                         saver_get_outdir(),  //
                         g_saver.prefix,      //
                         t,                   //
                         g_saver.image,       //
                         g_saver.format_name, //
                         counter,             //
                         "raw");
}

/// \brief Закрывает текущий файл, если он открыт.
///
/// \warning Вызывать только из saver-потока.
static void close_current_file(void)
{
    if (g_saver.current_file)
    {
        fclose(g_saver.current_file);
        g_saver.current_file = nullptr;
        log_info("Saved %s (frames=%u, size=%u bytes)", //
                 g_saver.current_filename, g_saver.frames_in_file, g_saver.frames_in_file * g_saver.frame_size);
    }
    g_saver.frames_in_file   = 0;
    g_saver.batch_start_time = 0;
}

/// \brief Открывает новый файл для записи.
///
/// \details Увеличивает file_counter, создаёт директорию (если нужно),
///          формирует имя и открывает файл. При ошибке fopen оставляет
///          current_file == nullptr — кадры в этот файл не пойдут.
///
/// \warning Вызывать только из saver-потока.
static void open_new_file(void)
{
    if (has_outdir())
    {
        struct stat st;
        if (stat(g_saver.outdir, &st) != 0)
        {
#ifdef _WIN32
            mkdir(g_saver.outdir);
#else
            mkdir(g_saver.outdir, SAVER_DIR_MODE_POSIX);
#endif
        }
    }

    g_saver.file_counter++;
    g_saver.batch_start_time = time(nullptr);
    g_saver.frames_in_file   = 0;

    build_filename(g_saver.current_filename, sizeof(g_saver.current_filename));

    g_saver.current_file = fopen(g_saver.current_filename, "wb");
    if (!g_saver.current_file)
    {
        log_error("fopen save");
        g_saver.batch_start_time = 0;
    }
}

/// \brief Пишет кадр в текущий файл, при необходимости открывает новый.
///
/// \details Условия ротации: файл не открыт, достигнут batch_size,
///          истёк time_window. Само копирование — fwrite.
///
/// \warning Вызывать только из saver-потока.
///
/// \param frame Буфер кадра (frame_size байт)
static void write_frame_to_file(uint8_t * frame)
{
    bool need_new = (g_saver.current_file == nullptr);

    if (!need_new && g_saver.frames_in_file >= g_saver.batch_size)
    {
        need_new = true;
    }
    if (!need_new && g_saver.time_window > 0 &&
        difftime(time(nullptr), g_saver.batch_start_time) >= (double)g_saver.time_window)
    {
        need_new = true;
    }

    if (need_new)
    {
        close_current_file();
        open_new_file();
    }

    if (g_saver.current_file)
    {
        fwrite(frame, 1, (size_t)g_saver.frame_size, g_saver.current_file);
        g_saver.frames_in_file++;
    }
}

/// \brief Функция saver-потока.
///
/// \details Крутится до stop_requested && очередь пуста. По каждому кадру
///          из очереди: запись на диск, затем возврат буфера в пул.
///          По flush_requested — закрытие текущего файла.
///
/// \param arg Не используется (требуется сигнатурой потока)
///
/// \return Win32: код выхода; POSIX: nullptr
#ifdef _WIN32
static DWORD WINAPI saver_thread_proc(LPVOID /*unused*/)
#else
static void * saver_thread_proc(void * /*unused*/)
#endif
{
    for (;;)
    {
        uint8_t * buf = nullptr;
        bool stop     = false;
        bool flush    = false;

        SAVER_MUTEX_LOCK(&g_saver.mutex);

        while (g_saver.queue.count == 0 && !g_saver.stop_requested && !g_saver.flush_requested)
        {
            SAVER_COND_WAIT(&g_saver.cond, &g_saver.mutex);
        }

        if (g_saver.queue.count > 0)
        {
            buf                = g_saver.queue.bufs[ g_saver.queue.head ];
            g_saver.queue.head = (uint8_t)((g_saver.queue.head + 1) % g_saver.pool.capacity);
            g_saver.queue.count--;
        }
        else if (g_saver.stop_requested)
        {
            stop = true;
        }
        else if (g_saver.flush_requested)
        {
            flush                   = true;
            g_saver.flush_requested = false;
        }

        SAVER_MUTEX_UNLOCK(&g_saver.mutex);

        if (stop)
        {
            break;
        }

        if (buf)
        {
            write_frame_to_file(buf);

            // Возврат буфера в пул.
            SAVER_MUTEX_LOCK(&g_saver.mutex);
            g_saver.pool.bufs[ g_saver.pool.free_count++ ] = buf;
            SAVER_MUTEX_UNLOCK(&g_saver.mutex);
        }

        if (flush)
        {
            close_current_file();
        }
    }

    return 0;
}

//---------------------------------------------------- Functions -------------------------------------------------------

void saver_init(saver_config_s * config)
{
    memset(&g_saver, 0, sizeof(g_saver));

    if (config->prefix && *config->prefix)
    {
        strncpy(g_saver.prefix, config->prefix, sizeof(g_saver.prefix) - 1);
        g_saver.recording = true;
    }
    else
    {
        strcpy(g_saver.prefix, SAVER_DEFAULT_PREFIX);
        g_saver.recording = false;
    }

    if (config->outdir)
    {
        strncpy(g_saver.outdir, config->outdir, sizeof(g_saver.outdir) - 1);
    }

    g_saver.batch_size  = config->batch_size > 0 ? (uint32_t)config->batch_size : 1u;
    g_saver.time_window = config->time_window > 0 ? (uint32_t)config->time_window : 0u;
    g_saver.image       = config->image;
    g_saver.frame_size  = config->image.x * config->image.y * config->bpp;

    if (config->format_name)
    {
        strncpy(g_saver.format_name, config->format_name, sizeof(g_saver.format_name) - 1);
    }

    // Выделяем пул одной аллокацией. При нехватке памяти половиним число
    // буферов, пока не дойдём до SAVER_POOL_MIN. Если и минимум не влез —
    // работаем без записи.
    uint8_t pool_size = SAVER_POOL_MAX;
    uint8_t * block   = nullptr;

    while (pool_size >= SAVER_POOL_MIN)
    {
        size_t bytes = g_saver.frame_size * pool_size;
        block        = malloc(bytes);
        if (block)
        {
            break;
        }
        log_warn("saver: could not allocate %u buffers (%zu bytes), trying %u\n", //
                 pool_size, bytes, pool_size / 2);
        pool_size = pool_size / 2;
    }

    if (!block)
    {
        log_warn("saver: failed to allocate frame pool (frame_size=%u bytes). Recording disabled.\n",
                 g_saver.frame_size);
        return;
    }

    // Раздаём указатели на срезы блока.
    g_saver.pool.block = block;
    for (uint8_t i = 0; i < pool_size; i++)
    {
        g_saver.pool.bufs[ i ] = block + ((size_t)i * (size_t)g_saver.frame_size);
    }
    g_saver.pool.capacity   = pool_size;
    g_saver.pool.free_count = pool_size;

    if (pool_size < SAVER_POOL_MAX)
    {
        log_warn("saver: frame pool reduced to %u buffers (%zu bytes total)\n", //
                 pool_size, g_saver.frame_size * pool_size);
    }

    SAVER_MUTEX_INIT(&g_saver.mutex);
    SAVER_COND_INIT(&g_saver.cond);

#ifdef _WIN32
    g_saver.thread = CreateThread(nullptr, 0, saver_thread_proc, nullptr, 0, nullptr);
    if (!g_saver.thread)
    {
        log_warn("saver: CreateThread failed (%lu)\n", (unsigned long)GetLastError());
        free(g_saver.pool.block);
        g_saver.pool.block = nullptr;
        SAVER_MUTEX_DESTROY(&g_saver.mutex);
        SAVER_COND_DESTROY(&g_saver.cond);
        return;
    }
#else
    if (pthread_create(&g_saver.thread, nullptr, saver_thread_proc, nullptr) != 0)
    {
        log_error("saver: pthread_create");
        free(g_saver.pool.block);
        g_saver.pool.block = nullptr;
        SAVER_MUTEX_DESTROY(&g_saver.mutex);
        SAVER_COND_DESTROY(&g_saver.cond);
        return;
    }
#endif

    g_saver.enabled = true;
}

/// \brief Добавляет кадр в очередь на запись.
///
/// \details Забирает буфер из пула, копирует кадр и кладёт буфер в очередь.
///          Если пул пуст — кадр дропается, функция возвращается сразу.
///          Потокобезопасна. Не блокирует вызывающий поток надолго.
///
/// \param frame_data Указатель на данные кадра (frame_size байт)
///
/// \note Если saver выключен (не хватило памяти) — no-op.
/// \note Если запись выключена (recording == false) — no-op.
void saver_add_frame(const uint8_t * frame_data)
{
    if (!g_saver.enabled)
    {
        return;
    }

    uint8_t * buf = nullptr;

    // Фаза 1: забрать буфер из пула.
    SAVER_MUTEX_LOCK(&g_saver.mutex);

    if (!g_saver.recording)
    {
        SAVER_MUTEX_UNLOCK(&g_saver.mutex);
        return;
    }

    if (g_saver.pool.free_count == 0)
    {
        // Пул пуст: saver-поток не успевает. Дропаем, main не блокируем.
        g_saver.dropped++;
        SAVER_MUTEX_UNLOCK(&g_saver.mutex);
        return;
    }

    buf = g_saver.pool.bufs[ --g_saver.pool.free_count ];
    SAVER_MUTEX_UNLOCK(&g_saver.mutex);

    // Фаза 2: копирование вне мьютекса — не блокируем saver-поток.
    memcpy(buf, frame_data, (size_t)g_saver.frame_size);

    // Фаза 3: положить буфер в очередь.
    SAVER_MUTEX_LOCK(&g_saver.mutex);

    // Между фазами recording мог выключиться — тогда возвращаем буфер в пул.
    if (!g_saver.recording || g_saver.stop_requested)
    {
        g_saver.pool.bufs[ g_saver.pool.free_count++ ] = buf;
        SAVER_MUTEX_UNLOCK(&g_saver.mutex);
        return;
    }

    // Инвариант: queue.count < pool.capacity, потому что сумма всех буферов
    // в пуле и очереди == pool.capacity, а один сейчас у нас в руках.
    g_saver.queue.bufs[ g_saver.queue.tail ] = buf;
    g_saver.queue.tail                       = (uint8_t)((g_saver.queue.tail + 1) % g_saver.pool.capacity);
    g_saver.queue.count++;

    SAVER_COND_SIGNAL(&g_saver.cond);
    SAVER_MUTEX_UNLOCK(&g_saver.mutex);
}

/// \brief Принудительно закрыть текущий файл.
///
/// \details Асинхронная операция: saver-поток дольёт очередь и закроет файл.
///          Возвращается сразу, не ждёт завершения. Следующий кадр откроет
///          новый файл.
void saver_flush(void)
{
    if (!g_saver.enabled)
    {
        return;
    }

    SAVER_MUTEX_LOCK(&g_saver.mutex);
    g_saver.flush_requested = true;
    SAVER_COND_SIGNAL(&g_saver.cond);
    SAVER_MUTEX_UNLOCK(&g_saver.mutex);
}

/// \brief Переключить режим записи (вкл/выкл).
///
/// \details При выключении ставит flush_requested — saver-поток закроет файл.
///          При включении новые кадры начнут писаться с нового файла.
void saver_toggle_recording(void)
{
    if (!g_saver.enabled)
    {
        return;
    }

    SAVER_MUTEX_LOCK(&g_saver.mutex);
    g_saver.recording  = !g_saver.recording;
    bool now_recording = g_saver.recording;
    if (!now_recording)
    {
        g_saver.flush_requested = true;
    }
    SAVER_COND_SIGNAL(&g_saver.cond);
    SAVER_MUTEX_UNLOCK(&g_saver.mutex);

    log_info("Recording %s\n", now_recording ? "enabled" : "disabled");
}

/// \brief Останавливает saver-поток и освобождает ресурсы.
///
/// \details Ставит stop_requested, дожидается завершения потока (он доработает
///          очередь), освобождает блок пула и мьютекс. Идемпотентна —
///          повторный вызов безвреден.
///
/// \warning Вызывать последней среди функций saver’а. После неё
///          g_saver.enabled == false, все операции становятся no-op.
void saver_cleanup(void)
{
    if (!g_saver.enabled)
    {
        return;
    }

    // Останавливаем поток. Он доработает очередь и выйдет.
    SAVER_MUTEX_LOCK(&g_saver.mutex);
    g_saver.stop_requested = true;
    SAVER_COND_SIGNAL(&g_saver.cond);
    SAVER_MUTEX_UNLOCK(&g_saver.mutex);

#ifdef _WIN32
    WaitForSingleObject(g_saver.thread, INFINITE);
    CloseHandle(g_saver.thread);
#else
    pthread_join(g_saver.thread, nullptr);
#endif

    // Поток уже слил очередь и вернул буферы в пул. Освобождаем
    // одну аллокацию — весь блок.
    free(g_saver.pool.block);
    g_saver.pool.block      = nullptr;
    g_saver.pool.free_count = 0;
    g_saver.pool.capacity   = 0;

    close_current_file();

    if (g_saver.dropped > 0)
    {
        log_warn("saver: dropped %llu frames (pool exhausted)\n", (unsigned long long)g_saver.dropped);
    }

    SAVER_MUTEX_DESTROY(&g_saver.mutex);
    SAVER_COND_DESTROY(&g_saver.cond);

    g_saver.enabled = false;
}

/// \brief Возвращает текущее состояние записи.
///
/// \return true, если запись активна
bool saver_is_recording(void)
{
    if (!g_saver.enabled)
    {
        return false;
    }

    SAVER_MUTEX_LOCK(&g_saver.mutex);
    bool r = g_saver.recording;
    SAVER_MUTEX_UNLOCK(&g_saver.mutex);
    return r;
}

/// \brief Возвращает выходную директорию.
///
/// \return Строка пути; никогда не nullptr; по умолчанию "."
const char * saver_get_outdir(void)
{
    return g_saver.outdir[ 0 ] ? g_saver.outdir : ".";
}

const char * saver_get_prefix(void)
{
    return g_saver.prefix[ 0 ] ? g_saver.prefix : SAVER_DEFAULT_PREFIX;
}

const char * saver_get_format_name(void)
{
    return g_saver.format_name[ 0 ] ? g_saver.format_name : "unknown";
}

//--------------------------------------------------- End Of File ------------------------------------------------------
