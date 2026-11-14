#include "defect_view.h"

#include "analysis/frame_context.h"
#include "app/settings.h"
#include "log/logger.h"
#include "render/overlay.h"
#include "render/ui/ui_color_scheme.h"
#include "render/viewport.h"
#include "save/saver.h"
#include "util/utils.h"

#include <stdio.h>
#include <time.h>

#define MAX_DEFECTS 4096

static bool g_needs_rescan = false;
static bool g_enabled      = false;
static defect_s g_defects[ MAX_DEFECTS ];
static int g_count     = 0;
static int g_dead_cnt  = 0;
static int g_stuck_cnt = 0;
static int g_hot_cnt   = 0;

// Forward declaration
static void defect_view_export(const char * filename);

void defect_view_cleanup(void)
{
    g_enabled      = false;
    g_needs_rescan = false;
    g_count        = 0;
    g_dead_cnt     = 0;
    g_stuck_cnt    = 0;
    g_hot_cnt      = 0;
}

void defect_view_init(void)
{
    bool on;
    if (settings_get_feature_bool("defect_view", "enabled", &on))
    {
        g_enabled      = on;
        g_needs_rescan = on; // rescan отложим до первого валидного кадра
    }
    log_info("defect_view: %s", g_enabled ? "enabled (rescan pending)" : "disabled");
}

static void add_defect(int x, int y, defect_type_e t, uint32_t v, int32_t d)
{
    if (g_count >= MAX_DEFECTS)
    {
        return;
    }
    g_defects[ g_count++ ] = (defect_s) {
        {x, y},
        t, v, d
    };
    switch (t)
    {
        case DEFECT_DEAD: g_dead_cnt++; break;
        case DEFECT_STUCK: g_stuck_cnt++; break;
        case DEFECT_HOT: g_hot_cnt++; break;
    }
}

static uint32_t sample_value(point_s p, int bpp)
{
    frame_pixel_t px = frame_context_read(p.x, p.y);
    return (bpp <= 2) ? px.raw : (uint32_t)px.luma;
}

void defect_view_rescan(void)
{
    g_count        = 0;
    g_dead_cnt     = 0;
    g_stuck_cnt    = 0;
    g_hot_cnt      = 0;
    g_needs_rescan = false;

    if (!frame_context_is_valid())
    {
        return;
    }

    image_size_s size = frame_context_size();
    int bpp           = frame_context_bpp();
    if (size.x < 3 || size.y < 3 || bpp == 0)
    {
        return;
    }

    uint32_t max_val = 255;
    if (bpp == 2)
    {
        max_val = 65535;
    }

    uint32_t dead_thr  = utils_max(1u, max_val / 64);
    uint32_t stuck_thr = utils_max(1u, max_val / 64);
    int32_t hot_thr    = (int32_t)utils_max(16u, max_val / 16);

    for (int y = 1; y < size.y - 1; y++)
    {
        for (int x = 1; x < size.x - 1; x++)
        {
            uint32_t cv = sample_value((point_s) {x, y}, bpp);

            if (cv <= dead_thr)
            {
                add_defect(x, y, DEFECT_DEAD, cv, 0);
                continue;
            }
            if (cv >= max_val - stuck_thr)
            {
                add_defect(x, y, DEFECT_STUCK, cv, 0);
                continue;
            }

            int64_t sum = 0;
            for (int dy = -1; dy <= 1; dy++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    if (dx == 0 && dy == 0)
                    {
                        continue;
                    }
                    sum += sample_value((point_s) {x + dx, y + dy}, bpp);
                }
            }
            int32_t avg    = (int32_t)(sum / 8);
            int32_t delta  = (int32_t)cv - avg;
            int32_t adelta = (delta < 0) ? -delta : delta;

            if (adelta > hot_thr)
            {
                add_defect(x, y, DEFECT_HOT, cv, delta);
            }
        }
    }

    log_info("Defect scan: %d found (dead %d, stuck %d, hot %d)", g_count, g_dead_cnt, g_stuck_cnt, g_hot_cnt);

    char csv_path[ 1024 ];
    utils_build_filename(csv_path, sizeof(csv_path), saver_get_outdir(), saver_get_prefix(), time(nullptr), size,
                         saver_get_format_name(), "defects", "csv");

    defect_view_export(csv_path);
    g_needs_rescan = false;
}

void defect_view_toggle(void)
{
    if (!g_enabled)
    {
        defect_view_rescan();
        g_enabled      = true;
        g_needs_rescan = false;
    }
    else
    {
        g_enabled = false;
    }
    settings_set_feature_bool("defect_view", "enabled", g_enabled);
    log_info("Defect overlay %s", g_enabled ? "enabled" : "disabled");
}

bool defect_view_is_enabled(void)
{
    return g_enabled;
}

void defect_view_draw(image_size_s win, float dpi_scale)
{
    if (g_needs_rescan && frame_context_is_valid())
    {
        defect_view_rescan();
    }

    if (!g_enabled || g_count == 0)
    {
        return;
    }

    // Маркеры
    for (int i = 0; i < g_count; i++)
    {
        point_s screen;
        if (!viewport_frame_to_screen((point_s) {g_defects[ i ].pos.x, g_defects[ i ].pos.y}, &screen))
        {
            continue;
        }

        fcolor_rgba_s color;
        switch (g_defects[ i ].type)
        {
            case DEFECT_DEAD: color = UI_DEFECT_DEAD; break;
            case DEFECT_STUCK: color = UI_DEFECT_STUCK; break;
            case DEFECT_HOT: color = UI_DEFECT_HOT; break;
            default: color = UI_RGBA_WHITE; break;
        }

        int r       = (int)(3 * dpi_scale);
        rect_s cell = {screen.x - r, screen.y - r, screen.x + r, screen.y + r};
        overlay_rect_outline_draw(cell, color, utils_max(1, (int)dpi_scale));
    }

    // Панель-сводка
    const float text_scale = dpi_scale * 1.0f;
    const int pad          = (int)(6 * dpi_scale);
    const int margin       = (int)(10 * dpi_scale);
    const int line_h       = (int)(14 * text_scale);

    char line[ 160 ];
    snprintf(line, sizeof(line), "Defects: %d  (dead %d, stuck %d, hot %d)", g_count, g_dead_cnt, g_stuck_cnt,
             g_hot_cnt);

    int tw = overlay_text_measure(line, text_scale);
    int tx = margin;
    int ty = win.y - (int)(32 * dpi_scale);

    rect_s panel = {tx - pad, ty - pad, tx + tw + pad, ty + line_h + pad};
    overlay_rect_draw(panel, UI_PANEL_BG_STRONG);
    overlay_text_draw((point_s) {tx, ty}, line, UI_TEXT_PRIMARY, text_scale);
}

//------------------------------------------------------- Экспорт в CSV ------------------------------------------------

static void defect_view_export(const char * filename)
{
    if (g_count == 0)
    {
        return;
    }

    FILE * f = fopen(filename, "w");
    if (!f)
    {
        log_error("defect_view: failed to create %s", filename);
        return;
    }

    fprintf(f, "x,y,type,value,delta\n");

    for (int i = 0; i < g_count; i++)
    {
        const char * type_str = "unknown";
        switch (g_defects[ i ].type)
        {
            case DEFECT_DEAD: type_str = "dead"; break;
            case DEFECT_STUCK: type_str = "stuck"; break;
            case DEFECT_HOT: type_str = "hot"; break;
            default: break;
        }
        fprintf(f, "%d,%d,%s,%u,%d\n", g_defects[ i ].pos.x, g_defects[ i ].pos.y, type_str, g_defects[ i ].value,
                g_defects[ i ].delta);
    }

    fclose(f);
    log_info("Defect list saved to %s", filename);
}
