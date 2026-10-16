#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "util/types.h"
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
typedef int socklen_t;
    #define SOURCE_CLOSE_SOCKET(s) closesocket(s)
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #define SOURCE_CLOSE_SOCKET(s) close(s)
#endif

/// \brief Состояние сборки одного кадра.
typedef struct frame_state_s
{
    image_size_s image;
    uint16_t bpp;
    uint32_t frame_size;
    uint32_t received_bytes;
    uint16_t last_seq;
    uint32_t last_timestamp;
    uint32_t packet_count;
    uint32_t lost_packets;
    bool marker_received;
    bool frame_ready;
    uint64_t last_packet_time_us;
} frame_state_s;

/// \brief Параметры открытия источника.
typedef struct source_params_s
{
    image_size_s image;
    int bpp;
    int port;          ///< для RTP
    const char * path; ///< для файла
    int mock_fps;      ///< для файла
} source_params_s;

typedef struct source_s source_s;

/// \brief Виртуальная таблица методов источника.
typedef struct source_ops_s
{
    bool (*open)(source_s * self, const source_params_s * params);
    void (*close)(source_s * self);
    int (*receive)(source_s * self, frame_state_s * state, uint8_t * buffer);
    bool (*check_timeout)(source_s * self, frame_state_s * state);
} source_ops_s;

/// \brief Экземпляр источника.
struct source_s
{
    const source_ops_s * ops;
    void * impl;
};

void frame_state_init(frame_state_s * state, image_size_s size, uint8_t bpp);
void frame_state_reset(frame_state_s * state);
bool frame_state_is_ready(const frame_state_s * state);
bool frame_state_check_timeout(frame_state_s * state);


bool source_open(source_s * src, const source_params_s * params);
void source_close(source_s * src);

int source_receive(source_s * src, frame_state_s * state, uint8_t * buffer);
bool source_check_timeout(source_s * src, frame_state_s * state);

