override MAKEFLAGS += -j --silent

CC = gcc
CP = cp
MKDIR_P = mkdir -p
RM_RF = rm -rf

TARGET = rtp_viewer.exe
SRC_DIR = src
OBJ_DIR = obj
PUBLISH_DIR = publish

SRCS =  $(SRC_DIR)/main.c
SRCS += $(SRC_DIR)/rtp.c
SRCS += $(SRC_DIR)/gl_render.c
SRCS += $(SRC_DIR)/gl_api.c
SRCS += $(SRC_DIR)/stats.c
SRCS += $(SRC_DIR)/saver.c
SRCS += $(SRC_DIR)/utils.c
SRCS += $(SRC_DIR)/png_writer.c

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CFLAGS  = -std=gnu2x -Wall -Wextra -O2 -g -Isrc -Ilib/glfw/include
LDFLAGS = -Llib/glfw/lib -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lws2_32

all: $(TARGET)

publish: $(TARGET)
	$(RM_RF) $(PUBLISH_DIR)
	$(MKDIR_P) $(PUBLISH_DIR)
	$(MKDIR_P) $(PUBLISH_DIR)/formats
	$(CP) $(TARGET) $(PUBLISH_DIR)/
	echo "rtp_viewer.exe -x 640 -y 512 -p 5004 -f ycbcr" >> $(PUBLISH_DIR)/ycbcb-640-512.bat
	$(CP) formats/*.fmt  $(PUBLISH_DIR)/formats/ 2>/dev/null || true
	$(CP) formats/*.glsl $(PUBLISH_DIR)/formats/ 2>/dev/null || true


$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	$(MKDIR_P) $(OBJ_DIR)

clean:
	$(RM_RF) $(OBJ_DIR) $(TARGET) $(PUBLISH_DIR)

.PHONY: all clean publish