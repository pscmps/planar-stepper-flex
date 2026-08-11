#include "planner.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "config.h"
#if MOTION_BACKEND_DRV8835
#include "current_monitor.h"
#include "position_sensors.h"
#endif
#include "stepper.h"

static volatile bool paused_flag = false;
static volatile bool cancelled_flag = false;
static volatile bool external_cancel_requested = false;
static volatile bool soft_reset_requested = false;

static void poll_realtime(void *context) {
#if MOTION_BACKEND_DRV8835
    current_monitor_service();
    position_sensors_service();
#endif
    controller_state_t *state = (controller_state_t *)context;
    if (external_cancel_requested) {
        cancelled_flag = true;
    }
    int ch = PICO_ERROR_TIMEOUT;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        switch (ch) {
        case '?':
            gcode_report_status(state);
            break;
        case '!':
            paused_flag = true;
            state->paused = true;
            break;
        case '~':
            paused_flag = false;
            state->paused = false;
            break;
        case 0x18:
            soft_reset_requested = true;
            cancelled_flag = true;
            paused_flag = false;
            state->paused = false;
            break;
        case 0x85:
            cancelled_flag = true;
            paused_flag = false;
            state->paused = false;
            break;
        default:
            break;
        }
    }
}

void planner_init(void) {
    stepper_init();
    planner_enable(false);
}

bool planner_enable(bool enabled) {
#if MOTION_BACKEND_DRV8835
    stepper_enable(enabled);
    return true;
#else
    if (enabled && !tmc2209_all_ready()) {
        stepper_enable(false);
        tmc2209_set_enabled(false);
        return false;
    }
    stepper_enable(enabled);
    tmc2209_set_enabled(enabled);
    return true;
#endif
}

void planner_reset_position(controller_state_t *state) {
    stepper_set_position(state->machine_x, state->machine_y);
}

bool planner_line_to(controller_state_t *state, float x, float y, float feed_mm_min, bool rapid) {
    (void)rapid;
    cancelled_flag = false;
    soft_reset_requested = false;
    if (external_cancel_requested) {
        cancelled_flag = true;
    }
    if (!state->enabled) {
        state->enabled = true;
        if (!planner_enable(true)) {
            state->enabled = false;
            state->alarm = true;
            return false;
        }
    }
    const bool completed = stepper_line_to(x, y, feed_mm_min, &paused_flag,
                                           &cancelled_flag, poll_realtime, state);
    external_cancel_requested = false;
    stepper_get_position(&state->machine_x, &state->machine_y);
    state->work_x = state->machine_x;
    state->work_y = state->machine_y;
    if (!completed) {
        state->enabled = false;
        planner_enable(false);
        if (soft_reset_requested) {
            gcode_state_init(state);
            stepper_set_position(0.0f, 0.0f);
            printf("\r\nGrbl 1.1h ['$' for help]\r\n");
        }
    }
    return completed || cancelled_flag;
}

bool planner_jog(controller_state_t *state, float dx, float dy, float feed_mm_min) {
    return planner_line_to(state, state->work_x + dx, state->work_y + dy, feed_mm_min, false);
}

void planner_dwell_ms(unsigned int milliseconds) {
    sleep_ms(milliseconds);
}

void planner_feed_hold(controller_state_t *state) {
    paused_flag = true;
    state->paused = true;
}

void planner_resume(controller_state_t *state) {
    paused_flag = false;
    state->paused = false;
}

void planner_cancel_jog(controller_state_t *state) {
    external_cancel_requested = false;
    cancelled_flag = true;
    paused_flag = false;
    state->paused = false;
    state->enabled = false;
    planner_enable(false);
}

void planner_request_cancel(void) {
    external_cancel_requested = true;
    cancelled_flag = true;
    paused_flag = false;
}

void planner_soft_reset(controller_state_t *state) {
    external_cancel_requested = false;
    cancelled_flag = true;
    paused_flag = false;
    gcode_state_init(state);
    stepper_set_position(0.0f, 0.0f);
    planner_enable(false);
}

void planner_service(void) {
    stepper_service();
#if MOTION_BACKEND_DRV8835
    current_monitor_service();
    position_sensors_service();
#endif
}
