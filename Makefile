CC = gcc
TARGET = rtp_play.exe
SRC_DIR = src
OBJ_DIR = obj

SRCS =  $(SRC_DIR)/main.c
SRCS += $(SRC_DIR)/rtp.c
SRCS += $(SRC_DIR)/gl_render.c
SRCS += $(SRC_DIR)/gl_api.c
SRCS += $(SRC_DIR)/stats.c
SRCS += $(SRC_DIR)/saver.c
SRCS += $(SRC_DIR)/utils.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CFLAGS  = -std=gnu23 -Wall -Wextra -O2 -g -Isrc -Ilib/glfw/include
LDFLAGS = -Llib/glfw/lib -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lws2_32

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean