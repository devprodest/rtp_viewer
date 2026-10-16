
#pragma once

#include <stdbool.h>
#include <stdint.h>

void stats_init(const char * logfile);
void stats_cleanup(void);
void stats_update(void);

void stats_add_packets(int count);

void stats_frame_rendered(void);
float stats_get_fps(void);