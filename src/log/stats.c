#include "stats.h"

#include "log/logger.h"
#include "render/renderer.h"
#include "util/utils.h"

#include <inttypes.h>

static uint64_t g_total_packets       = 0;
static uint64_t g_total_frames        = 0;
static uint64_t g_last_second_packets = 0;
static uint64_t g_last_second_frames  = 0;
static uint64_t g_last_time_s         = 0;

static double fps = 0.0;

void stats_init(const char * logfile)
{
    (void)logfile;
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
    g_total_packets       += (uint64_t)count;
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

        renderer_refresh_title();
        log_info("Packets: %" PRIu64 " (%.0f pkts/s), Frames: %" PRIu64 " (%.1f fps)", g_total_packets, pps,
                 g_total_frames, fps);
    }

    g_last_second_packets = 0;
    g_last_second_frames  = 0;
    g_last_time_s         = now;
}

double stats_get_fps(void)
{
    return fps;
}