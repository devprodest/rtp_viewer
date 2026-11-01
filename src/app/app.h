#pragma once

typedef struct app_config_s app_config_s;

int app_run(const app_config_s * cfg);

bool app_toggle_pause(void);
bool app_step_forward(void);
bool app_step_backward(void);
bool app_is_paused(void);
