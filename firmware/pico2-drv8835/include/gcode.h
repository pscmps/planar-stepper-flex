#pragma once

#include <stdbool.h>

typedef struct {
    bool absolute;
    float work_x;
    float work_y;
    float work_z;
    float machine_x;
    float machine_y;
    float machine_z;
    float feed_mm_min;
    int pen_pwm;
    bool pen_up;
    bool enabled;
    bool paused;
    bool alarm;
} controller_state_t;

void gcode_state_init(controller_state_t *state);
void gcode_process_line(char *line, controller_state_t *state);
void gcode_process_realtime(int ch, controller_state_t *state);
void gcode_report_status(const controller_state_t *state);
