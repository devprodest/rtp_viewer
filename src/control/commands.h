#pragma once

#include <stdbool.h>

typedef enum commands_e
{
    CMD_SMOOTHING_TOGGLE,
    CMD_SATURATION_TOGGLE,
    CMD_HISTOGRAM_TOGGLE,
    CMD_HISTOGRAM_SCALE_TOGGLE,
    CMD_LINE_PROFILE_TOGGLE,
    CMD_INSPECTOR_TOGGLE,
    CMD_GRADIENT_TOGGLE,
    CMD_VECTORSCOPE_TOGGLE,
    CMD_STATS_TOGGLE,
    CMD_FULLSCREEN_TOGGLE,
    CMD_AUTO_LEVELS_TOGGLE,
    CMD_DEFECT_VIEW_TOGGLE,
    CMD_DIFF_SET_REFERENCE,
    CMD_DIFF_TOGGLE,
    CMD_DIFF_GAIN_UP,
    CMD_DIFF_GAIN_DOWN,
    CMD_PLAY_PAUSE_TOGGLE,
    CMD_PLAY_STEP_FORWARD,
    CMD_PLAY_STEP_BACKWARD,
    CMD_RECORD_TOGGLE,
    CMD_RECORD_SCREENSHOT,
    CMD_RECORD_FLUSH,
    CMD_RESET_ALL,
    CMD_ZOOM_RESET,
    CMD_SHOW_HELP,
} commands_e;

typedef enum menu_group_e
{
    MENU_GROUP_DISPLAY,
    MENU_GROUP_ANALYSIS,
    MENU_GROUP_DIFF,
    MENU_GROUP_PLAYBACK,
    MENU_GROUP_RECORDING,
    MENU_GROUP_UTILITY,
} menu_group_e;

typedef enum command_type_e
{
    CMD_TYPE_NONE,   ///< Пустая (неинициализированная) ячейка
    CMD_TYPE_ACTION, ///< Одноразовое действие
    CMD_TYPE_TOGGLE, ///< Переключатель
} command_type_e;

typedef struct command_info_s
{
    const char * label;
    const char * hotkey_str;
    int key;
    int mods_required;
    command_type_e type;
    menu_group_e group;
} command_info_s;

// --- Реестр ---
const command_info_s * cmd_get_all(int * out_count);
const command_info_s * cmd_get_info(commands_e cmd);

// --- Управление ---
void cmd_execute(commands_e cmd);
bool cmd_is_active(commands_e cmd);
