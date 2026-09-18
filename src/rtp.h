// rtp.h - приём RTP (RFC 4175)

#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define RTP_CLOSE_SOCKET(s) closesocket(s)
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define RTP_CLOSE_SOCKET(s) close(s)
#endif

// Состояние сборки кадра
typedef struct rtp_frame_state_s
{
    uint16_t width;
    uint16_t height;
    uint16_t bytes_per_pixel;
    uint32_t frame_size;
    uint32_t received_bytes;
    uint16_t last_seq;
    uint32_t last_timestamp;
    uint32_t packet_count;
    uint32_t lost_packets;
    bool marker_received;
    bool frame_ready;
    uint64_t last_packet_time_us;
} rtp_frame_state_s;

int rtp_socket_open(int port);
void rtp_frame_state_init(rtp_frame_state_s *state, uint16_t width, uint16_t height, uint16_t bytes_per_pixel);
void rtp_frame_state_reset(rtp_frame_state_s *state);

static inline bool rtp_frame_is_ready(const rtp_frame_state_s *state)
{
    return state->frame_ready;
}

bool rtp_frame_check_timeout(rtp_frame_state_s *state);
int rtp_receive_packets(int sock, rtp_frame_state_s *state, uint8_t *frame_buffer);

#endif