#include "stats_overlay.h"

#include "analysis/analytics.h"
#include "render/overlay.h"
#include "render/ui/panel.h"
#include "render/ui/ui_color_scheme.h"

#include <stdio.h>

static point_s g_pos = {-1, -1};

point_s stats_overlay_get_pos(void)
{
    return g_pos;
}
void stats_overlay_set_pos(point_s p)
{
    g_pos = p;
}

image_size_s stats_overlay_get_size(float dpi_scale)
{
    const analytics_stats_s * st = analytics_get_stats();
    int channels                 = (st && st->valid) ? st->channels : 1;
    int line_h                   = (int)(14 * dpi_scale);
    int panel_w                  = (int)(380 * dpi_scale);
    int panel_h                  = channels * line_h + 8;
    return (image_size_s) {panel_w, panel_h};
}

void stats_overlay_draw(point_s pos, float dpi_scale)
{
    if (!analytics_is_stats_visible())
    {
        return;
    }

    const analytics_stats_s * st = analytics_get_stats();
    if (!st || !st->valid)
    {
        return;
    }

    if (pos.x < 0 || pos.y < 0)
    {
        pos   = (point_s) {(int)(10 * dpi_scale), (int)(10 * dpi_scale)};
        g_pos = pos;
    }

    int title_h = panel_title_height(dpi_scale);
    int x       = pos.x;
    int y       = pos.y + title_h;

    const float text_scale = dpi_scale * 1.f;
    const int line_h       = (int)(14 * text_scale);
    const int lines        = (st->channels == 1) ? 1 : 3;

    char line[ 192 ];

    // Фон — ровно как в stats_overlay_get_size: channels * line_h + 8
    const int panel_h = lines * line_h + 8;
    const int panel_w = (int)(380 * dpi_scale);

    rect_s panel = {
        .x0 = x,
        .y0 = y,
        .x1 = x + panel_w,
        .y1 = y + panel_h,
    };
    overlay_rect_draw(panel, UI_PANEL_BG);

    // Текст с 4-пиксельным отступом сверху, чтобы не вылезти за panel_h
    if (st->channels == 1)
    {
        snprintf(line, sizeof(line), "RAW min=%.0f max=%.0f mean=%.1f std=%.1f", st->min[ 0 ], st->max[ 0 ],
                 st->mean[ 0 ], st->std[ 0 ]);
        overlay_text_draw((point_s) {x + 6, y + 4}, line, UI_TEXT_PRIMARY, text_scale);
    }
    else
    {
        static const char * names[ 3 ] = {"R", "G", "B"};
        for (int c = 0; c < 3; c++)
        {
            snprintf(line, sizeof(line), "%s min=%.0f max=%.0f mean=%.1f std=%.1f", names[ c ], st->min[ c ],
                     st->max[ c ], st->mean[ c ], st->std[ c ]);
            overlay_text_draw((point_s) {x + 6, y + 4 + c * line_h}, line, UI_TEXT_PRIMARY, text_scale);
        }
    }
}
