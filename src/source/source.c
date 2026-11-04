#include "source.h"

#include "util/utils.h"

#include <string.h>

#define FRAME_TIMEOUT_US 500000ull

void frame_state_init(frame_state_s * state, image_size_s size, uint8_t bpp)
{
    memset(state, 0, sizeof(*state));
    state->image               = size;
    state->bpp                 = bpp;
    state->frame_size          = state->image.x * state->image.y * bpp;
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

bool frame_state_is_ready(const frame_state_s * state)
{
    return state->frame_ready;
}

bool source_seek_to(source_s * src, int target_frame)
{
    if (!src || !src->ops || !src->ops->seek_to)
    {
        return false;
    }
    return src->ops->seek_to(src, target_frame);
}

bool source_step_forward(source_s * src)
{
    if (!src || !src->ops || !src->ops->step_forward)
    {
        return false;
    }
    return src->ops->step_forward(src);
}

bool source_step_backward(source_s * src)
{
    if (!src || !src->ops || !src->ops->step_backward)
    {
        return false;
    }
    return src->ops->step_backward(src);
}

bool source_get_frame_position(source_s * src, int * current, int * total)
{
    if (!src || !src->ops || !src->ops->get_frame_position)
    {
        return false;
    }
    return src->ops->get_frame_position(src, current, total);
}
