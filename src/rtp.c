
#include "rtp.h"
#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <fcntl.h>
#endif

#define RTP_FIXED_HEADER_SIZE     12
#define RTP_4175_PAYLOAD_HDR_SIZE 8
#define RTP_MAX_PACKET_SIZE       65536
#define RTP_FRAME_TIMEOUT_US      500000ull

// ---- сокет ----

int rtp_socket_open(int port)
{
#ifdef _WIN32
    static bool wsa_inited = false;
    if (!wsa_inited)
    {
        if (WSAStartup(MAKEWORD(2, 2), &(WSADATA) {}) != 0)
        {
            fprintf(stderr, "WSAStartup failed\n");
            return -1;
        }
        wsa_inited = true;
    }
#endif

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (sock == (int)INVALID_SOCKET)
    {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
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
        RTP_CLOSE_SOCKET(sock);
        return -1;
    }

    fprintf(stderr, "UDP socket bound to port %d\n", port);

#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        perror("fcntl nonblock");
        RTP_CLOSE_SOCKET(sock);
        return -1;
    }
#endif

    return sock;
}

// ---- состояние кадра ----

void rtp_frame_state_init(rtp_frame_state_s * state, uint16_t width, uint16_t height, uint16_t bytes_per_pixel)
{
    memset(state, 0, sizeof(*state));
    state->width               = width;
    state->height              = height;
    state->bytes_per_pixel     = bytes_per_pixel;
    state->frame_size          = (uint32_t)width * height * bytes_per_pixel;
    state->last_packet_time_us = get_time_us();
}

void rtp_frame_state_reset(rtp_frame_state_s * state)
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

bool rtp_frame_check_timeout(rtp_frame_state_s * state)
{
    if (state->received_bytes == 0)
    {
        return false;
    }

    uint64_t now = get_time_us();
    if ((now - state->last_packet_time_us) > RTP_FRAME_TIMEOUT_US)
    {
        state->frame_ready = true;
        return true;
    }
    return false;
}

bool rtp_frame_is_ready(const rtp_frame_state_s * state)
{
    return state->frame_ready;
}

// ---- разбор ----

static inline uint16_t be16(const uint8_t * p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t be32(const uint8_t * p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// Обработать один RTP-пакет. Возвращает 1 если пакет учтён, 0 если пропущен.
static int process_rtp_packet(const uint8_t * packet, size_t plen, rtp_frame_state_s * state, uint8_t * frame_buffer)
{
    if (plen < RTP_FIXED_HEADER_SIZE)
    {
        return 0;
    }

    uint8_t b0 = packet[0];

    uint8_t version = (b0 >> 6) & 0x03;
    if (version != 2)
    {
        return 0;
    }

    uint8_t csrc_count = b0 & 0x0F;

    uint8_t b1  = packet[1];
    bool marker = (b1 >> 7) & 0x01;

    uint16_t seq       = be16(packet + 2);
    uint32_t timestamp = be32(packet + 4);

    // Пропускаем RTP header (12 + CC*4) и 2 байта extseq
    size_t hdr_pos = RTP_FIXED_HEADER_SIZE + ((size_t)csrc_count * 4) + 2;

    // Разбираем цепочку rawline-заголовков
    uint16_t rl_lengths[16];
    uint16_t rl_lines[16];
    uint16_t rl_offsets[16];
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

        rl_lengths[rl_count] = rl_len;
        rl_lines[rl_count]   = rl_f_lineno & 0x7FFF;
        rl_offsets[rl_count] = rl_c_offset & 0x7FFF;
        rl_count++;

        hdr_pos += 6;

        if (!more)
        {
            break;
        }
    }

    // Суммарный payload по всем rawline
    uint32_t total_payload = 0;
    for (int i = 0; i < rl_count; i++)
    {
        total_payload += rl_lengths[i];
    }

    if (hdr_pos + total_payload > plen)
    {
        return 0;
    }

    const uint8_t * payload = packet + hdr_pos;

    // Граница кадра по timestamp
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
            uint16_t diff        = (uint16_t)(seq - expected);
            state->lost_packets += diff;
        }
        state->last_seq = seq;
    }

    // Раскладываем payload по каждому rawline
    uint32_t running = 0;
    for (int i = 0; i < rl_count; i++)
    {
        uint32_t stride      = (uint32_t)state->width * state->bytes_per_pixel;
        uint32_t byte_offset = (uint32_t)rl_offsets[i] * state->bytes_per_pixel;
        uint32_t base_offset = (rl_lines[i] * stride) + byte_offset;

        if ((uint64_t)base_offset + rl_lengths[i] > state->frame_size)
        {
            continue;
        }

        memcpy(frame_buffer + base_offset, payload + running, rl_lengths[i]);
        running += rl_lengths[i];
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

// ---- основной приём ----

int rtp_receive_packets(int sock, rtp_frame_state_s * state, uint8_t * frame_buffer)
{
    uint8_t packet[RTP_MAX_PACKET_SIZE];
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
            fprintf(stderr, "recvfrom failed: %d\n", err);
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
