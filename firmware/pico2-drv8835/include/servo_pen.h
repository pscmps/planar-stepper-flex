#pragma once

#include <stdbool.h>

#include "servo_pen_math.h"

void servo_pen_safe_init(void);
bool servo_pen_configure(const servo_pen_config_t *config);
void servo_pen_get_config(servo_pen_config_t *config);
bool servo_pen_set_pulse_us(unsigned int pulse_us);
bool servo_pen_set_z(float z, bool *changed);
bool servo_pen_move_up(bool *changed);
bool servo_pen_move_down(bool *changed);
unsigned int servo_pen_current_pulse_us(void);
bool servo_pen_is_up(void);
