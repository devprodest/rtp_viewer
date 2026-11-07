#pragma once

#include <stdbool.h>

typedef enum commands_e
{
    CMD_TOGGLE_SMOOTHING,
    CMD_TOGGLE_SATURATION,
    CMD_TOGGLE_HISTOGRAM,
    CMD_TOGGLE_HISTOGRAM_SCALE,
    CMD_TOGGLE_LINE_PROFILE,
    CMD_TOGGLE_RECORDING,
    CMD_TOGGLE_INSPECTOR,
    CMD_TOGGLE_GRADIENT,
    CMD_TOGGLE_VECTORSCOPE,
    CMD_TOGGLE_STATS,
    CMD_TOGGLE_FULLSCREEN,
    CMD_TOGGLE_AUTO_LEVELS,
    CMD_TOGGLE_PAUSE,
    CMD_SET_REFERENCE,
    CMD_TOGGLE_DIFF,
    CMD_DIFF_GAIN_UP,
    CMD_DIFF_GAIN_DOWN,
    CMD_STEP_FORWARD,
    CMD_STEP_BACKWARD,
    CMD_REQUEST_SCREENSHOT,
    CMD_FLUSH_SAVER,
    CMD_CLEAR_SELECTION,
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
