#include "settings.h"

#include "log/logger.h"
#include "util/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PANELS       8
#define SAVE_THROTTLE_US 1000000ULL // 1 секунда

typedef struct
{
    char id[ 32 ];
    point_s pos;
    bool has_pos;
    bool enabled;
    bool has_enabled;
} panel_setting_t;

static panel_setting_t g_panels[ MAX_PANELS ];
static int g_panel_count = 0;

static bool g_dirty         = false;
static uint64_t g_last_save = 0;
static char g_ini_path[ 1024 ];

//------------------------------------------------------- Внутреннее ---------------------------------------------------

static panel_setting_t * find_or_create(const char * id)
{
    for (int i = 0; i < g_panel_count; i++)
    {
        if (strcmp(g_panels[ i ].id, id) == 0)
        {
            return &g_panels[ i ];
        }
    }
    if (g_panel_count >= MAX_PANELS)
    {
        return nullptr;
    }
    panel_setting_t * p = &g_panels[ g_panel_count++ ];
    memset(p, 0, sizeof(*p));
    snprintf(p->id, sizeof(p->id), "%s", id);
    return p;
}

static void build_ini_path(void)
{
    char exe_dir[ 512 ];
    if (utils_get_exe_dir(exe_dir, sizeof(exe_dir)))
    {
        snprintf(g_ini_path, sizeof(g_ini_path), "%s/rtp_viewer.ini", exe_dir);
    }
    else
    {
        snprintf(g_ini_path, sizeof(g_ini_path), "rtp_viewer.ini");
    }
}

/// \brief Парсит строку "panel.histogram.x=470" в id/key/value.
static bool parse_line(const char * line, char * id, char * key, long * val)
{
    // Пропускаем пробелы и комментарии
    while (*line == ' ' || *line == '\t')
    {
        line++;
    }
    if (*line == '\0' || *line == '#' || *line == ';')
    {
        return false;
    }

    // Ищем "panel.<id>.<key>=<val>"
    if (strncmp(line, "panel.", 6) != 0)
    {
        return false;
    }
    line += 6;

    const char * dot = strchr(line, '.');
    if (!dot)
    {
        return false;
    }

    size_t id_len = (size_t)(dot - line);
    if (id_len == 0 || id_len >= 32)
    {
        return false;
    }
    memcpy(id, line, id_len);
    id[ id_len ] = '\0';

    line            = dot + 1;
    const char * eq = strchr(line, '=');
    if (!eq)
    {
        return false;
    }

    size_t key_len = (size_t)(eq - line);
    if (key_len == 0 || key_len >= 16)
    {
        return false;
    }
    memcpy(key, line, key_len);
    key[ key_len ] = '\0';

    *val = strtol(eq + 1, nullptr, 10);
    return true;
}

//------------------------------------------------------- API ---------------------------------------------------------

void settings_load(void)
{
    build_ini_path();
    g_panel_count = 0;
    g_dirty       = false;

    FILE * f = fopen(g_ini_path, "r");
    if (!f)
    {
        log_info("settings: %s not found, using defaults", g_ini_path);
        return;
    }

    char line[ 256 ];
    while (fgets(line, sizeof(line), f))
    {
        char id[ 32 ], key[ 16 ];
        long val;
        if (!parse_line(line, id, key, &val))
        {
            continue;
        }

        panel_setting_t * p = find_or_create(id);
        if (!p)
        {
            continue;
        }

        if (strcmp(key, "x") == 0)
        {
            p->pos.x   = (int)val;
            p->has_pos = true;
        }
        else if (strcmp(key, "y") == 0)
        {
            p->pos.y   = (int)val;
            p->has_pos = true;
        }
        else if (strcmp(key, "enabled") == 0)
        {
            p->enabled     = (val != 0);
            p->has_enabled = true;
        }
    }
    fclose(f);
    log_info("settings: loaded %d panel entries from %s", g_panel_count, g_ini_path);
}

void settings_save(void)
{
    if (!g_dirty)
    {
        return;
    }

    FILE * f = fopen(g_ini_path, "w");
    if (!f)
    {
        log_error("settings: failed to write %s", g_ini_path);
        return;
    }

    fprintf(f, "# RTP Viewer settings\n");
    fprintf(f, "# Auto-generated. Delete to reset to defaults.\n\n");

    for (int i = 0; i < g_panel_count; i++)
    {
        panel_setting_t * p = &g_panels[ i ];
        if (p->has_pos)
        {
            fprintf(f, "panel.%s.x=%d\n", p->id, p->pos.x);
            fprintf(f, "panel.%s.y=%d\n", p->id, p->pos.y);
        }
        if (p->has_enabled)
        {
            fprintf(f, "panel.%s.enabled=%d\n", p->id, p->enabled ? 1 : 0);
        }
    }

    fclose(f);
    g_dirty     = false;
    g_last_save = get_time_us();
    log_info("settings: saved to %s", g_ini_path);
}

void settings_mark_dirty(void)
{
    g_dirty = true;
}

void settings_tick(void)
{
    if (!g_dirty)
    {
        return;
    }

    uint64_t now = get_time_us();
    if (now - g_last_save >= SAVE_THROTTLE_US)
    {
        settings_save();
    }
}

bool settings_get_panel_pos(const char * id, point_s * out_pos)
{
    for (int i = 0; i < g_panel_count; i++)
    {
        if (strcmp(g_panels[ i ].id, id) == 0 && g_panels[ i ].has_pos)
        {
            *out_pos = g_panels[ i ].pos;
            return true;
        }
    }
    return false;
}

bool settings_get_panel_enabled(const char * id, bool * out_enabled)
{
    for (int i = 0; i < g_panel_count; i++)
    {
        if (strcmp(g_panels[ i ].id, id) == 0 && g_panels[ i ].has_enabled)
        {
            *out_enabled = g_panels[ i ].enabled;
            return true;
        }
    }
    return false;
}

void settings_set_panel_pos(const char * id, point_s pos)
{
    panel_setting_t * p = find_or_create(id);
    if (!p)
    {
        return;
    }
    p->pos     = pos;
    p->has_pos = true;
    settings_mark_dirty();
}

void settings_set_panel_enabled(const char * id, bool enabled)
{
    panel_setting_t * p = find_or_create(id);
    if (!p)
    {
        return;
    }
    p->enabled     = enabled;
    p->has_enabled = true;
    settings_mark_dirty();
}
