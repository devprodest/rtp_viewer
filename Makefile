override MAKEFLAGS += -j --silent

PROJECT_NAME = rtp_viewer

CC      = gcc
CP      = cp
MKDIR_P = mkdir -p
RM_RF   = rm -rf
RC      = windres

LIB_DIR      = lib
SRC_DIR      = src
RES_DIR      = $(SRC_DIR)/res
OBJ_DIR      = obj
PUBLISH_DIR  = publish



ARCHIVE  = $(PROJECT_NAME)_$(VERSION_STR).zip
TARGET   = $(PROJECT_NAME).exe

# --- Исходные файлы ---
SRCS  = $(SRC_DIR)/main.c
SRCS += $(SRC_DIR)/app/app.c
SRCS += $(SRC_DIR)/app/config.c
SRCS += $(SRC_DIR)/app/pipeline.c
SRCS += $(SRC_DIR)/analysis/analytics.c
SRCS += $(SRC_DIR)/analysis/histogram.c
SRCS += $(SRC_DIR)/analysis/selection.c
SRCS += $(SRC_DIR)/analysis/sharpness.c
SRCS += $(SRC_DIR)/control/commands.c
SRCS += $(SRC_DIR)/control/hotkeys.c
SRCS += $(SRC_DIR)/control/mouse.c
SRCS += $(SRC_DIR)/format/format_config.c
SRCS += $(SRC_DIR)/help/help.c
SRCS += $(SRC_DIR)/log/logger.c
SRCS += $(SRC_DIR)/log/stats.c
SRCS += $(SRC_DIR)/render/format_pass.c
SRCS += $(SRC_DIR)/render/gl/gl_api.c
SRCS += $(SRC_DIR)/render/gl/gl_utils.c
SRCS += $(SRC_DIR)/render/post_pass.c
SRCS += $(SRC_DIR)/render/renderer.c
SRCS += $(SRC_DIR)/render/overlay.c
SRCS += $(SRC_DIR)/render/window.c
SRCS += $(SRC_DIR)/render/viewport.c
SRCS += $(SRC_DIR)/render/ui/selection_view.c
SRCS += $(SRC_DIR)/render/ui/histogram_view.c
SRCS += $(SRC_DIR)/render/ui/vectorscope_view.c
SRCS += $(SRC_DIR)/render/ui/window_title.c
SRCS += $(SRC_DIR)/render/ui/context_menu.c
SRCS += $(SRC_DIR)/render/ui/progress_bar.c
SRCS += $(SRC_DIR)/render/ui/svg_icons.c
SRCS += $(SRC_DIR)/render/ui/pixel_probe.c
SRCS += $(SRC_DIR)/render/ui/line_profile_view.c
SRCS += $(SRC_DIR)/render/ui/diff_view.c
SRCS += $(SRC_DIR)/save/saver.c
SRCS += $(SRC_DIR)/save/screenshot.c
SRCS += $(SRC_DIR)/save/png_writer.c
SRCS += $(SRC_DIR)/source/source_factory.c
SRCS += $(SRC_DIR)/source/source_file.c
SRCS += $(SRC_DIR)/source/source_rtp.c
SRCS += $(SRC_DIR)/source/source.c
SRCS += $(SRC_DIR)/util/utils.c

# Lib
SRCS += $(LIB_DIR)/nanovg/src/nanovg.c

INCS += $(SRC_DIR)
INCS += $(LIB_DIR)
INCS += $(LIB_DIR)/nanovg/src
INCS += $(LIB_DIR)/nanosvg/src
INCS += $(LIB_DIR)/glfw/include

# --- Windows resources ---
RES_OBJ = $(OBJ_DIR)/version.o
VERSION_RC = $(RES_DIR)/version.rc

# --- Ресурсы (#embed) ---
SVG_ICONS = $(wildcard $(RES_DIR)/icons/*.svg)
FONT_FILE = $(RES_DIR)/RobotoMono-Regular.ttf

# --- Объектные файлы ---
OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS)) $(RES_OBJ)


# --- Версия ---
include version.mk
VERSION_STR   = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)
# Флаги версии для C-кода
VER_DEFS = -DRTP_VIEWER_VERSION_MAJOR=$(VERSION_MAJOR) \
           -DRTP_VIEWER_VERSION_MINOR=$(VERSION_MINOR) \
           -DRTP_VIEWER_VERSION_PATCH=$(VERSION_PATCH) \
           -DRTP_VIEWER_VERSION_STR=\"$(VERSION_STR)\"

CFLAGS  = -std=gnu2x -Wall -Wextra -Oz -MMD -MP $(addprefix -I,$(INCS)) $(VER_DEFS)
LDFLAGS = -Llib/glfw/lib -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 -lws2_32 -lwinmm -flto -s

all: $(TARGET)

# ---------------------------------------------------------------------------
# publish
# ---------------------------------------------------------------------------
publish: $(TARGET) version.mk
	@echo "Publishing rtp_viewer $(VERSION_STR)"
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

# --- Правила сборки ---

# Передаем те же флаги версии в windres для version.rc
$(RES_OBJ): $(VERSION_RC)
	@$(MKDIR_P) $(@D)
	$(RC) -I $(SRC_DIR) $(VER_DEFS) -i $< --input-format=rc --output-format=coff -o $@

# Специальное правило для nanovg (отключаем warnings)
$(OBJ_DIR)/$(LIB_DIR)/nanovg/src/nanovg.o: $(LIB_DIR)/nanovg/src/nanovg.c
	@$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -w -c $< -o $@

# Общее правило для всех C файлов
$(OBJ_DIR)/%.o: %.c
	@$(MKDIR_P) $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

# --- Явные зависимости для #embed ---
$(OBJ_DIR)/$(SRC_DIR)/render/ui/svg_icons.o: $(SVG_ICONS)
$(OBJ_DIR)/$(SRC_DIR)/render/overlay.o: $(FONT_FILE)

# --- Включаем сгенерированные зависимости от заголовков ---
-include $(OBJS:.o=.d)

print-%: ; @echo $*=$($*)

clean:
	$(RM_RF) $(OBJ_DIR) $(TARGET) $(PUBLISH_DIR) $(ARCHIVE)

.PHONY: all clean publish