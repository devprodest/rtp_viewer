#include "source.h"

#include "util/utils.h"

#include <string.h>

#define FRAME_TIMEOUT_US 500000ull

void frame_state_init(frame_state_s * state, image_size_s size, uint8_t bytes_per_pixel)
{
    memset(state, 0, sizeof(*state));
    state->image               = size;
    state->bytes_per_pixel     = bytes_per_pixel;
    state->frame_size          = state->image.x * state->image.y * bytes_per_pixel;
    state->last_packet_time_us = get_time_us();
}

void frame_state_reset(frame_state_s * state)
{
    state->received_bytes      = 0;
    state->last_seq            = 0;
    state->last_timestamp      = 0;
    state->packet_count        = 0;
    state->lost_packets        = 0;
    state->marker_received     = false;
    state->frame_ready         = false;
    state->last_packet_time_us = get_time_us();
}

bool frame_state_check_timeout(frame_state_s * state)
{
    if (state->received_bytes == 0)
    {
        return false;
    }

    uint64_t now = get_time_us();
    if ((now - state->last_packet_time_us) > FRAME_TIMEOUT_US)
    {
        state->frame_ready = true;
        return true;
    }
    return false;
}

bool source_open(source_s * src, const source_params_s * params)
{
    if (!src || !src->ops || !src->ops->open)
    {
        return false;
    }
    return src->ops->open(src, params);
}

void source_close(source_s * src)
{
    if (src && src->ops && src->ops->close)
    {
        src->ops->close(src);
    }
}

int source_receive(source_s * src, frame_state_s * state, uint8_t * buffer)
{
    if (!src || !src->ops || !src->ops->receive)
    {
        return -1;
    }
    return src->ops->receive(src, state, buffer);
}

bool source_check_timeout(source_s * src, frame_state_s * state)
{
    if (!src || !src->ops || !src->ops->check_timeout)
    {
        return false;
    }
    return src->ops->check_timeout(src, state);
}
