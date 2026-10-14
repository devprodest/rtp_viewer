
#include "stats.h"
#include "render/gl_render.h"
#include "util/utils.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char * g_logfile         = nullptr;
static uint64_t g_total_packets       = 0;
static uint64_t g_total_frames        = 0;
static uint64_t g_last_second_packets = 0;
static uint64_t g_last_second_frames  = 0;
static uint64_t g_last_time_s         = 0;

static double fps = 0.0;

void stats_init(const char * logfile)
{
    g_logfile             = logfile;
    g_total_packets       = 0;
    g_total_frames        = 0;
    g_last_second_packets = 0;
    g_last_second_frames  = 0;
    g_last_time_s         = get_time_s();
}

void stats_cleanup(void)
{
}

void stats_add_packets(int count)
{
    g_total_packets += (uint64_t)count;
    g_last_second_packets += (uint64_t)count;
}

void stats_frame_rendered(void)
{
    g_total_frames++;
    g_last_second_frames++;
}

void stats_update(void)
{
    uint64_t now = get_time_s();
    if (now == g_last_time_s)
    {
        return;
    }

    uint64_t delta = now - g_last_time_s;
    if (delta > 0)
    {
        fps        = (double)g_last_second_frames / (double)delta;
        double pps = (double)g_last_second_packets / (double)delta;

        gl_render_refresh_title();
        printf("Packets: %" PRIu64 " (%.0f pkts/s), Frames: %" PRIu64 " (%.1f fps)\n", g_total_packets, pps,
               g_total_frames, fps);

        if (g_logfile)
        {
            FILE * f = fopen(g_logfile, "a");
            if (f)
            {
                time_t t       = time(nullptr);
                struct tm * tm = localtime(&t);
                char time_str[ 64 ];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
                fprintf(f, "[%s] Total packets: %" PRIu64 ", PPS: %.0f, Frames: %" PRIu64 ", FPS: %.1f\n", time_str,
                        g_total_packets, pps, g_total_frames, fps);
                fclose(f);
            }
        }
    }

    g_last_second_packets = 0;
    g_last_second_frames  = 0;
    g_last_time_s         = now;
}

double stats_get_fps(void)
{
    return fps;
}
