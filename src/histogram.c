#include "histogram.h"

#include <string.h>

void histogram_reset(histogram_s * h)
{
    memset(h, 0, sizeof(*h));
    h->active_channels = 1;
}

void histogram_add(histogram_s * h, int channel, uint32_t bin)
{
    if (channel >= 0 && channel < HIST_CHANNELS && bin < HIST_BINS)
    {
        h->bins[channel][bin]++;
    }
}

void histogram_finalize(histogram_s * h)
{
    for (int c = 0; c < HIST_CHANNELS; c++)
    {
        h->max_per_channel[c] = 1;
    }

    for (int c = 0; c < h->active_channels; c++)
    {
        for (int i = 0; i < HIST_BINS; i++)
        {
            if (h->bins[c][i] > h->max_per_channel[c])
            {
                h->max_per_channel[c] = h->bins[c][i];
            }
        }
    }
}