override MAKEFLAGS += -j --silent

PROJECT_NAME = rtp_viewer

CC      = gcc
CP      = cp
MKDIR_P = mkdir -p
RM_RF   = rm -rf
RC      = windres

LIB_DIR      = lib
SRC_DIR      = src
OBJ_DIR      = obj
PUBLISH_DIR  = publish

# Версия тянется из единственного источника правды — src/version.h.
# sed вытаскивает значение RTP_VIEWER_VERSION_STR "...".
VERSION := $(shell sed -n 's/.*RTP_VIEWER_VERSION_STR *"\([^"]*\)".*/\1/p' $(SRC_DIR)/util/version.h)
ARCHIVE  = $(PROJECT_NAME)_$(VERSION).zip
TARGET   = $(PROJECT_NAME).exe

SRCS  = $(SRC_DIR)/main.c
SRCS += $(SRC_DIR)/analysis/analytics.c
SRCS += $(SRC_DIR)/analysis/histogram.c
SRCS += $(SRC_DIR)/analysis/selection.c
SRCS += $(SRC_DIR)/analysis/sharpness.c
SRCS += $(SRC_DIR)/app/app.c
SRCS += $(SRC_DIR)/app/config.c
SRCS += $(SRC_DIR)/control/commands.c
SRCS += $(SRC_DIR)/control/hotkeys.c
SRCS += $(SRC_DIR)/control/mouse.c
SRCS += $(SRC_DIR)/format/format_config.c
SRCS += $(SRC_DIR)/help/help.c
SRCS += $(SRC_DIR)/log/logger.c
SRCS += $(SRC_DIR)/log/stats.c
SRCS += $(SRC_DIR)/render/format_pass.c
SRCS += $(SRC_DIR)/render/gl_api.c
SRCS += $(SRC_DIR)/render/gl_utils.c
SRCS += $(SRC_DIR)/render/histogram_view.c
SRCS += $(SRC_DIR)/render/png_writer.c
SRCS += $(SRC_DIR)/render/post_pass.c
SRCS += $(SRC_DIR)/render/renderer.c
SRCS += $(SRC_DIR)/render/selection_view.c
SRCS += $(SRC_DIR)/render/overlay.c
SRCS += $(SRC_DIR)/render/window.c
SRCS += $(SRC_DIR)/render/viewport.c
SRCS += $(SRC_DIR)/render/ui/window_title.c
SRCS += $(SRC_DIR)/render/ui/context_menu.c
SRCS += $(SRC_DIR)/render/ui/progress_bar.c
SRCS += $(SRC_DIR)/render/ui/svg_icons.c
SRCS += $(SRC_DIR)/save/saver.c
SRCS += $(SRC_DIR)/save/screenshot.c
SRCS += $(SRC_DIR)/source/source_factory.c
SRCS += $(SRC_DIR)/source/source_file.c
SRCS += $(SRC_DIR)/source/source_rtp.c
SRCS += $(SRC_DIR)/source/source.c
SRCS += $(SRC_DIR)/util/utils.c
SRCS += $(LIB_DIR)/nanovg/src/nanovg.c

INCS += $(SRC_DIR)
INCS += $(LIB_DIR)
INCS += $(LIB_DIR)/nanovg/src
INCS += $(LIB_DIR)/nanosvg/src
INCS += $(LIB_DIR)/glfw/include

# --- Windows resources ---
RES_OBJ = obj/version.o
VERSION_SRC = $(SRC_DIR)/res/version.rc

OBJS = $(addprefix $(OBJ_DIR)/,$(notdir $(SRCS:.c=.o))) $(RES_OBJ)
vpath %.c $(sort $(dir $(SRCS)))

CFLAGS  = -std=gnu2x -Wall -Wextra -Oz -g $(addprefix -I,$(INCS))
LDFLAGS = -Llib/glfw/lib -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lws2_32 -lwinmm

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
	$(MKDIR_P) $(PUBLISH_DIR)/tmp
	@echo "$(TARGET) -x 640 -y 512 -f ycbcr -p 5004" > $(PUBLISH_DIR)/tmp/ycbcr-640-512.bat
	$(RM_RF) $(ARCHIVE)
	powershell -NoProfile -Command "Compress-Archive -Path '$(TARGET)' -DestinationPath '$(PUBLISH_DIR)/$(ARCHIVE)' -Update"
	powershell -NoProfile -Command "Compress-Archive -Path 'formats' -DestinationPath '$(PUBLISH_DIR)/$(ARCHIVE)' -Update"
	powershell -NoProfile -Command "Compress-Archive -Path '$(PUBLISH_DIR)/tmp/*.bat' -DestinationPath '$(PUBLISH_DIR)/$(ARCHIVE)' -Update"
	@echo "Archive created: $(ARCHIVE)"

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	size $(TARGET)

$(RES_OBJ): $(VERSION_SRC) $(SRC_DIR)/util/version.h | $(OBJ_DIR)
	$(RC) -I $(SRC_DIR) -i $< --input-format=rc --output-format=coff -o $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	$(MKDIR_P) $(OBJ_DIR)

obj/nanovg.o: lib/nanovg/src/nanovg.c
	$(CC) $(CFLAGS) -w -c $< -o $@

print-%: ; @echo $*=$($*)

clean:
	$(RM_RF) $(OBJ_DIR) $(TARGET) $(PUBLISH_DIR) $(ARCHIVE)

.PHONY: all clean publish