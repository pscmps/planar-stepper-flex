#pragma once

#include <stdbool.h>

#include "gcode.h"

void planner_init(void);
bool planner_enable(bool enabled);
void planner_reset_position(controller_state_t *state);
bool planner_line_to(controller_state_t *state, float x, float y, float feed_mm_min, bool rapid);
bool planner_jog(controller_state_t *state, float dx, float dy, float feed_mm_min);
void planner_dwell_ms(unsigned int milliseconds);
void planner_feed_hold(controller_state_t *state);
void planner_resume(controller_state_t *state);
void planner_cancel_jog(controller_state_t *state);
void planner_request_cancel(void);
void planner_soft_reset(controller_state_t *state);
void planner_service(void);
