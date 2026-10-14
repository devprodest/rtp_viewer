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
VERSION := $(shell sed -n 's/.*RTP_VIEWER_VERSION_STR *"\([^"]*\)".*/\1/p' $(SRC_DIR)/util/version.h)
ARCHIVE  = rtp_viewer_$(VERSION).zip

SRCS  = $(SRC_DIR)/main.c
SRCS += $(SRC_DIR)/source/source_rtp.c
SRCS += $(SRC_DIR)/source/source_file.c
SRCS += $(SRC_DIR)/render/gl_render.c
SRCS += $(SRC_DIR)/render/gl_api.c
SRCS += $(SRC_DIR)/render/text_overlay.c
SRCS += $(SRC_DIR)/render/png_writer.c
SRCS += $(SRC_DIR)/log/stats.c
SRCS += $(SRC_DIR)/save/saver.c
SRCS += $(SRC_DIR)/util/utils.c
SRCS += $(SRC_DIR)/help/help_dialog.c
SRCS += $(SRC_DIR)/analysis/histogram.c
SRCS += $(SRC_DIR)/control/mouse.c

# --- Windows resources ---
RES_OBJ = obj/version.o
VERSION_SRC = $(SRC_DIR)/res/version.rc

OBJS = $(addprefix $(OBJ_DIR)/,$(notdir $(SRCS:.c=.o))) $(RES_OBJ)
vpath %.c $(sort $(dir $(SRCS)))

CFLAGS  = -std=gnu2x -Wall -Wextra -O2 -g -Isrc -Ilib -Ilib/glfw/include
LDFLAGS = -Llib/glfw/lib -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lws2_32

all: $(TARGET)

# ---------------------------------------------------------------------------
# publish
#   1. чистит publish/
#   2. копирует туда exe, форматы и батник запуска
#   3. упаковывает publish/ в rtp_viewer_<version>.zip
# ---------------------------------------------------------------------------

publish: $(TARGET) $(SRC_DIR)/util/version.h
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

$(RES_OBJ): $(VERSION_SRC) $(SRC_DIR)/util/version.h | $(OBJ_DIR)
	$(RC) -I $(SRC_DIR) -i $< --input-format=rc --output-format=coff -o $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	$(MKDIR_P) $(OBJ_DIR)


print-%: ; @echo $*=$($*)

clean:
	$(RM_RF) $(OBJ_DIR) $(TARGET) $(PUBLISH_DIR) $(ARCHIVE)

.PHONY: all clean publish