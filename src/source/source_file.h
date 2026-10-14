#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "source_rtp.h"

bool file_source_open(const char * path, int width, int height, int bytes_per_pixel, int mock_fps);

void file_source_close(void);
int file_source_receive_frame(rtp_frame_state_s * state, uint8_t * buffer);
bool file_source_check_timeout(rtp_frame_state_s * state);
