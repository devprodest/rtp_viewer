#include "source_rtp.h"

#include "log/logger.h"
#include "util/utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <fcntl.h>
#endif

#define RTP_FIXED_HEADER_SIZE 12
#define RTP_MAX_PACKET_SIZE   65536

typedef struct rtp_impl_s
{
    int sock;
} rtp_impl_t;

static rtp_impl_t g_rtp_impl;
static source_s g_rtp_source;

static int open_socket(int port);
static void close_socket(int sock);
static int rtp_receive(source_s * self, frame_state_s * state, uint8_t * frame_buffer);
static bool rtp_check_timeout(source_s * self, frame_state_s * state);
static bool rtp_open(source_s * self, const source_params_s * params);
static void rtp_close(source_s * self);

static const source_ops_s g_rtp_ops = {
    .open          = rtp_open,
    .close         = rtp_close,
    .receive       = rtp_receive,
    .check_timeout = rtp_check_timeout,
};

static inline uint16_t be16(const uint8_t * p)
{
    return (uint16_t)(((uint16_t)p[ 0 ] << 8) | (uint16_t)p[ 1 ]);
}

static inline uint32_t be32(const uint8_t * p)
{
    return ((uint32_t)p[ 0 ] << 24) | ((uint32_t)p[ 1 ] << 16) | ((uint32_t)p[ 2 ] << 8) | (uint32_t)p[ 3 ];
}

static bool rtp_open(source_s * self, const source_params_s * params)
{
    rtp_impl_t * impl = (rtp_impl_t *)self->impl;
    impl->sock        = open_socket(params->port);
    return impl->sock >= 0;
}

static void rtp_close(source_s * self)
{
    rtp_impl_t * impl = (rtp_impl_t *)self->impl;
    if (impl->sock >= 0)
    {
        close_socket(impl->sock);
        impl->sock = -1;
    }
}

static int process_rtp_packet(const uint8_t * packet, size_t plen, frame_state_s * state, uint8_t * frame_buffer)
{
    if (plen < RTP_FIXED_HEADER_SIZE)
    {
        return 0;
    }

    uint8_t b0 = packet[ 0 ];

    uint8_t version = (b0 >> 6) & 0x03;
    if (version != 2)
    {
        return 0;
    }

    uint8_t csrc_count = b0 & 0x0F;

    uint8_t b1  = packet[ 1 ];
    bool marker = (b1 >> 7) & 0x01;

    uint16_t seq       = be16(packet + 2);
    uint32_t timestamp = be32(packet + 4);

    size_t hdr_pos = RTP_FIXED_HEADER_SIZE + ((size_t)csrc_count * 4) + 2;

    uint16_t rl_lengths[ 16 ];
    uint16_t rl_lines[ 16 ];
    uint16_t rl_offsets[ 16 ];
    int rl_count = 0;

    while (1)
    {
        if (hdr_pos + 6 > plen || rl_count >= 16)
        {
            return 0;
        }

        uint16_t rl_len      = be16(packet + hdr_pos + 0);
        uint16_t rl_f_lineno = be16(packet + hdr_pos + 2);
        uint16_t rl_c_offset = be16(packet + hdr_pos + 4);

        bool more = (rl_c_offset & 0x8000) != 0;

        rl_lengths[ rl_count ] = rl_len;
        rl_lines[ rl_count ]   = rl_f_lineno & 0x7FFF;
        rl_offsets[ rl_count ] = rl_c_offset & 0x7FFF;
        rl_count++;

        hdr_pos += 6;

        if (!more)
        {
            break;
        }
    }

    uint32_t total_payload = 0;
    for (int i = 0; i < rl_count; i++)
    {
        total_payload += rl_lengths[ i ];
    }

    if (hdr_pos + total_payload > plen)
    {
        return 0;
    }

    const uint8_t * payload = packet + hdr_pos;

    if (state->received_bytes > 0 && timestamp != state->last_timestamp)
    {
        state->frame_ready = true;
        return 0;
    }

    if (state->received_bytes == 0)
    {
        state->frame_ready    = false;
        state->last_seq       = seq;
        state->last_timestamp = timestamp;
        state->packet_count   = 0;
        state->lost_packets   = 0;
    }
    else
    {
        uint16_t expected = (uint16_t)(state->last_seq + 1);
        if (seq != expected)
        {
            uint16_t diff = (uint16_t)(seq - expected);
            state->lost_packets += diff;
        }
        state->last_seq = seq;
    }

    uint32_t running = 0;
    for (int i = 0; i < rl_count; i++)
    {
        uint32_t stride      = (uint32_t)state->image.x * state->bpp;
        uint32_t byte_offset = (uint32_t)rl_offsets[ i ] * state->bpp;
        uint32_t base_offset = (rl_lines[ i ] * stride) + byte_offset;

        if ((uint64_t)base_offset + rl_lengths[ i ] > state->frame_size)
        {
            continue;
        }

        memcpy(frame_buffer + base_offset, payload + running, rl_lengths[ i ]);
        running += rl_lengths[ i ];
    }

    state->received_bytes += total_payload;
    state->packet_count++;
    state->last_packet_time_us = get_time_us();

    if (marker)
    {
        state->marker_received = true;
        state->frame_ready     = true;
    }

    return 1;
}

static int rtp_receive(source_s * self, frame_state_s * state, uint8_t * frame_buffer)
{
    rtp_impl_t * impl = (rtp_impl_t *)self->impl;
    int sock          = impl->sock;

    uint8_t packet[ RTP_MAX_PACKET_SIZE ];
    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);
    int total          = 0;

    while (true)
    {
        ssize_t n = recvfrom(sock, (void *)packet, sizeof(packet), 0, (struct sockaddr *)&src_addr, &addr_len);
#ifdef _WIN32
        if (n == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK)
            {
                break;
            }
            log_error("rtp: recvfrom failed: %d\n", err);
            return -1;
        }
#else
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            perror("recvfrom");
            return -1;
        }
#endif

        if (process_rtp_packet(packet, (size_t)n, state, frame_buffer))
        {
            total++;
        }

        if (state->frame_ready)
        {
            break;
        }
    }

    return total;
}

static bool rtp_check_timeout(source_s * self, frame_state_s * state)
{
    (void)self;
    return frame_state_check_timeout(state);
}

static int open_socket(int port)
{
#ifdef _WIN32
    static bool wsa_inited = false;
    if (!wsa_inited)
    {
        if (WSAStartup(MAKEWORD(2, 2), &(WSADATA) {}) != 0)
        {
            log_error("rtp: WSAStartup failed\n");
            return -1;
        }
        wsa_inited = true;
    }
#endif

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (sock == (int)INVALID_SOCKET)
    {
        log_error("rtp: socket() failed: %d\n", WSAGetLastError());
        return -1;
    }
#else
    if (sock < 0)
    {
        perror("socket");
        return -1;
    }
#endif

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(int));

    int rcvbuf = 16 * 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (void *)&rcvbuf, sizeof(int));

    int bind_result = bind(sock,
                           (struct sockaddr *)&(struct sockaddr_in) {
                               .sin_family      = AF_INET,
                               .sin_addr.s_addr = INADDR_ANY,
                               .sin_port        = htons((uint16_t)port),
                           },
                           sizeof(struct sockaddr_in));
    if (bind_result < 0)
    {
        perror("bind");
        SOURCE_CLOSE_SOCKET(sock);
        return -1;
    }

    log_error("rtp: UDP socket bound to port %d\n", port);

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("fcntl nonblock");
        SOURCE_CLOSE_SOCKET(sock);
        return -1;
    }
#endif

    return sock;
}

static void close_socket(int sock)
{
    SOURCE_CLOSE_SOCKET(sock);
}

source_s * source_rtp_create(void)
{
    g_rtp_impl.sock   = -1;
    g_rtp_source.ops  = &g_rtp_ops;
    g_rtp_source.impl = &g_rtp_impl;
    return &g_rtp_source;
}
