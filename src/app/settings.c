#include "settings.h"

#include "log/logger.h"
#include "util/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PANELS         8
#define MAX_FEATURE_PARAMS 64
#define SAVE_THROTTLE_US   1000000ULL

typedef struct
{
    char id[ 32 ];
    point_s pos;
    bool has_pos;
    bool enabled;
    bool has_enabled;
    int anchor_h;
    int anchor_v;
} panel_setting_t;

typedef struct
{
    char feature[ 32 ];
    char key[ 32 ];
    char value[ 32 ];
} feature_param_t;

static panel_setting_t g_panels[ MAX_PANELS ];
static int g_panel_count = 0;

static feature_param_t g_features[ MAX_FEATURE_PARAMS ];
static int g_feature_count = 0;

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

static feature_param_t * find_feature(const char * feature, const char * key)
{
    for (int i = 0; i < g_feature_count; i++)
    {
        if (strcmp(g_features[ i ].feature, feature) == 0 && strcmp(g_features[ i ].key, key) == 0)
        {
            return &g_features[ i ];
        }
    }
    return nullptr;
}

static feature_param_t * find_or_create_feature(const char * feature, const char * key)
{
    feature_param_t * p = find_feature(feature, key);
    if (p)
    {
        return p;
    }
    if (g_feature_count >= MAX_FEATURE_PARAMS)
    {
        return nullptr;
    }
    p = &g_features[ g_feature_count++ ];
    memset(p, 0, sizeof(*p));
    snprintf(p->feature, sizeof(p->feature), "%s", feature);
    snprintf(p->key, sizeof(p->key), "%s", key);
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

/// \brief Парсит "panel.<id>.<key>=<value>" (value как строка).
static bool parse_line(const char * line, char * id, char * key, const char ** val_str)
{
    while (*line == ' ' || *line == '\t')
    {
        line++;
    }
    if (*line == '\0' || *line == '#' || *line == ';')
    {
        return false;
    }

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

    const char * v = eq + 1;
    while (*v == ' ' || *v == '\t')
    {
        v++;
    }
    *val_str = v;
    return true;
}

/// \brief Парсит "feature.<feature>.<key>=<value>".
static bool parse_feature_line(const char * line, char * feature, char * key, const char ** val_str)
{
    while (*line == ' ' || *line == '\t')
    {
        line++;
    }
    if (*line == '\0' || *line == '#' || *line == ';')
    {
        return false;
    }

    if (strncmp(line, "feature.", 8) != 0)
    {
        return false;
    }
    line += 8;

    const char * dot = strchr(line, '.');
    if (!dot)
    {
        return false;
    }

    size_t f_len = (size_t)(dot - line);
    if (f_len == 0 || f_len >= 32)
    {
        return false;
    }
    memcpy(feature, line, f_len);
    feature[ f_len ] = '\0';

    line            = dot + 1;
    const char * eq = strchr(line, '=');
    if (!eq)
    {
        return false;
    }

    size_t key_len = (size_t)(eq - line);
    if (key_len == 0 || key_len >= 32)
    {
        return false;
    }
    memcpy(key, line, key_len);
    key[ key_len ] = '\0';

    const char * v = eq + 1;
    while (*v == ' ' || *v == '\t')
    {
        v++;
    }
    *val_str = v;
    return true;
}

static int parse_anchor_h(const char * s)
{
    return (strncmp(s, "right", 5) == 0) ? 1 : 0;
}

static int parse_anchor_v(const char * s)
{
    return (strncmp(s, "bottom", 6) == 0) ? 1 : 0;
}

static const char * anchor_h_str(int v)
{
    return v ? "right" : "left";
}

static const char * anchor_v_str(int v)
{
    return v ? "bottom" : "top";
}

//------------------------------------------------------- API ---------------------------------------------------------

void settings_load(void)
{
    build_ini_path();
    g_panel_count   = 0;
    g_feature_count = 0;
    g_dirty         = false;

    FILE * f = fopen(g_ini_path, "r");
    if (!f)
    {
        log_info("settings: %s not found, using defaults", g_ini_path);
        return;
    }

    char line[ 256 ];
    while (fgets(line, sizeof(line), f))
    {
        char id[ 32 ], key[ 32 ];
        const char * val_str;

        if (parse_feature_line(line, id, key, &val_str))
        {
            feature_param_t * fp = find_or_create_feature(id, key);
            if (fp)
            {
                size_t len = strlen(val_str);
                while (len > 0 && (val_str[ len - 1 ] == '\n' || val_str[ len - 1 ] == '\r' ||
                                   val_str[ len - 1 ] == ' ' || val_str[ len - 1 ] == '\t'))
                {
                    len--;
                }
                if (len >= sizeof(fp->value))
                {
                    len = sizeof(fp->value) - 1;
                }
                memcpy(fp->value, val_str, len);
                fp->value[ len ] = '\0';
            }
            continue;
        }

        if (!parse_line(line, id, key, &val_str))
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
            p->pos.x   = atoi(val_str);
            p->has_pos = true;
        }
        else if (strcmp(key, "y") == 0)
        {
            p->pos.y   = atoi(val_str);
            p->has_pos = true;
        }
        else if (strcmp(key, "enabled") == 0)
        {
            p->enabled     = (atoi(val_str) != 0);
            p->has_enabled = true;
        }
        else if (strcmp(key, "anchor_h") == 0)
        {
            p->anchor_h = parse_anchor_h(val_str);
        }
        else if (strcmp(key, "anchor_v") == 0)
        {
            p->anchor_v = parse_anchor_v(val_str);
        }
    }
    fclose(f);
    log_info("settings: loaded %d panel entries, %d feature params from %s", g_panel_count, g_feature_count,
             g_ini_path);
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
        fprintf(f, "panel.%s.anchor_h=%s\n", p->id, anchor_h_str(p->anchor_h));
        fprintf(f, "panel.%s.anchor_v=%s\n", p->id, anchor_v_str(p->anchor_v));
    }

    if (g_feature_count > 0)
    {
        fprintf(f, "\n");
        for (int i = 0; i < g_feature_count; i++)
        {
            fprintf(f, "feature.%s.%s=%s\n", g_features[ i ].feature, g_features[ i ].key, g_features[ i ].value);
        }
    }

    fclose(f);
    g_dirty     = false;
    g_last_save = get_time_us();
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

bool settings_get_panel_anchor(const char * id, int * out_h, int * out_v)
{
    for (int i = 0; i < g_panel_count; i++)
    {
        if (strcmp(g_panels[ i ].id, id) == 0)
        {
            *out_h = g_panels[ i ].anchor_h;
            *out_v = g_panels[ i ].anchor_v;
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

void settings_set_panel_anchor(const char * id, int h, int v)
{
    panel_setting_t * p = find_or_create(id);
    if (!p)
    {
        return;
    }
    p->anchor_h = h;
    p->anchor_v = v;
    settings_mark_dirty();
}

//------------------------------------------------------- Feature params -----------------------------------------------

bool settings_get_feature_int(const char * feature, const char * key, int * out)
{
    feature_param_t * p = find_feature(feature, key);
    if (!p || p->value[ 0 ] == '\0')
    {
        return false;
    }
    if (out)
    {
        *out = atoi(p->value);
    }
    return true;
}

void settings_set_feature_int(const char * feature, const char * key, int val)
{
    feature_param_t * p = find_or_create_feature(feature, key);
    if (!p)
    {
        return;
    }
    snprintf(p->value, sizeof(p->value), "%d", val);
    settings_mark_dirty();
}

bool settings_get_feature_double(const char * feature, const char * key, double * out)
{
    feature_param_t * p = find_feature(feature, key);
    if (!p || p->value[ 0 ] == '\0')
    {
        return false;
    }
    if (out)
    {
        *out = strtod(p->value, nullptr);
    }
    return true;
}

void settings_set_feature_double(const char * feature, const char * key, double val)
{
    feature_param_t * p = find_or_create_feature(feature, key);
    if (!p)
    {
        return;
    }
    snprintf(p->value, sizeof(p->value), "%.10g", val);
    settings_mark_dirty();
}

bool settings_get_feature_bool(const char * feature, const char * key, bool * out)
{
    feature_param_t * p = find_feature(feature, key);
    if (!p || p->value[ 0 ] == '\0')
    {
        return false;
    }
    if (out)
    {
        *out = (atoi(p->value) != 0);
    }
    return true;
}

void settings_set_feature_bool(const char * feature, const char * key, bool val)
{
    feature_param_t * p = find_or_create_feature(feature, key);
    if (!p)
    {
        return;
    }
    snprintf(p->value, sizeof(p->value), "%d", val ? 1 : 0);
    settings_mark_dirty();
}
