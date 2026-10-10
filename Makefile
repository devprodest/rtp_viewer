override MAKEFLAGS += -j --silent

CC      = gcc
CP      = cp
MKDIR_P = mkdir -p
RM_RF   = rm -rf
RC      = windres

TARGET       = rtp_viewer.exe
SRC_DIR      = src
OBJ_DIR      = obj
PUBLISH_DIR  = publish

# Версия тянется из единственного источника правды — src/version.h.
# sed вытаскивает значение RTP_VIEWER_VERSION_STR "...".
VERSION := $(shell sed -n 's/.*RTP_VIEWER_VERSION_STR *"\([^"]*\)".*/\1/p' $(SRC_DIR)/version.h)
ARCHIVE  = rtp_viewer_$(VERSION).zip

SRCS := $(wildcard $(SRC_DIR)/*.c)

# --- Windows resources ---
RES_OBJ = obj/version.o

OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) $(RES_OBJ)

CFLAGS  = -std=gnu2x -Wall -Wextra -O2 -g -Isrc -Ilib/glfw/include -Ilib
LDFLAGS = -Llib/glfw/lib -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lws2_32

all: $(TARGET)

# ---------------------------------------------------------------------------
# publish
#   1. чистит publish/
#   2. копирует туда exe, форматы и батник запуска
#   3. упаковывает publish/ в rtp_viewer_<version>.zip
# ---------------------------------------------------------------------------

publish: $(TARGET) $(SRC_DIR)/version.h
	@echo "Publishing rtp_viewer $(VERSION)"
	$(RM_RF) $(PUBLISH_DIR)
	$(MKDIR_P) $(PUBLISH_DIR)/formats
	$(CP) $(TARGET) $(PUBLISH_DIR)/
	@echo "rtp_viewer.exe -x 640 -y 512 -p 5004 -f ycbcr" > $(PUBLISH_DIR)/ycbcr-640-512.bat
	$(CP) formats/*.fmt  $(PUBLISH_DIR)/formats/ 2>/dev/null || true
	$(CP) formats/*.glsl $(PUBLISH_DIR)/formats/ 2>/dev/null || true
	$(RM_RF) $(ARCHIVE)
	powershell -NoProfile -Command "Compress-Archive -Path '$(PUBLISH_DIR)/rtp_viewer.exe' -DestinationPath '$(PUBLISH_DIR)/$(ARCHIVE)' -Update"
	powershell -NoProfile -Command "Compress-Archive -Path '$(PUBLISH_DIR)/formats' -DestinationPath '$(PUBLISH_DIR)/$(ARCHIVE)' -Update"
	powershell -NoProfile -Command "Compress-Archive -Path '$(PUBLISH_DIR)/*.bat' -DestinationPath '$(PUBLISH_DIR)/$(ARCHIVE)' -Update"
	@echo "Archive created: $(ARCHIVE)"

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(RES_OBJ): $(SRC_DIR)/version.rc $(SRC_DIR)/version.h | $(OBJ_DIR)
	$(RC) -I $(SRC_DIR) --input-format=rc --output-format=coff -i $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	$(MKDIR_P) $(OBJ_DIR)

clean:
	$(RM_RF) $(OBJ_DIR) $(TARGET) $(PUBLISH_DIR) $(ARCHIVE)

.PHONY: all clean publish