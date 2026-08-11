#include "gcode.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/gpio.h"

#include "config.h"
#if MOTION_BACKEND_DRV8835
#include "current_monitor.h"
#include "drv8835_motion_math.h"
#include "position_sensors.h"
#include "servo_pen.h"
#endif
#include "planner.h"
#include "stepper.h"

static char *trim(char *text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return text;
}

static void strip_comments(char *line) {
    bool paren = false;
    char *out = line;
    for (char *in = line; *in; in++) {
        if (*in == ';') {
            break;
        }
        if (*in == '(') {
            paren = true;
            continue;
        }
        if (*in == ')') {
            paren = false;
            continue;
        }
        if (!paren) {
            *out++ = (char)toupper((unsigned char)*in);
        }
    }
    *out = '\0';
}

static bool word_value(const char *line, char letter, float *value) {
    for (const char *p = line; *p; p++) {
        if (*p == letter) {
            char *end = NULL;
            float parsed = strtof(p + 1, &end);
            if (end != p + 1) {
                *value = parsed;
                return true;
            }
        }
    }
    return false;
}

static bool has_prefix(const char *line, const char *prefix) {
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

#if !MOTION_BACKEND_DRV8835
static bool ensure_diagnostic_drivers_ready(void) {
    const bool x_ready = tmc2209_ensure_ready(TMC_ADDR_X);
    const bool y_ready = tmc2209_ensure_ready(TMC_ADDR_Y);
    return x_ready && y_ready;
}

static bool enable_diagnostic_axis(char axis) {
    if (!tmc2209_all_ready() || !stepper_enable_axis(axis, true)) {
        stepper_enable(false);
        tmc2209_set_enabled(false);
        return false;
    }
    tmc2209_set_enabled(true);
    return true;
}
#endif

void gcode_state_init(controller_state_t *state) {
    memset(state, 0, sizeof(*state));
    state->absolute = true;
    state->feed_mm_min = DEFAULT_FEED_MM_MIN;
#if MOTION_BACKEND_DRV8835
    state->pen_pwm = (int)servo_pen_current_pulse_us();
    state->pen_up = servo_pen_is_up();
    state->work_z = 1.0f;
    state->machine_z = 1.0f;
#else
    state->pen_pwm = 0;
#endif
}

void gcode_report_status(const controller_state_t *state) {
    const char *mode = state->alarm ? "Alarm" : state->paused ? "Hold" : "Idle";
    printf(
        "<%s|WPos:%.3f,%.3f,%.3f|MPos:%.3f,%.3f,%.3f|WCO:%.3f,%.3f,%.3f",
        mode,
        state->work_x,
        state->work_y,
        state->work_z,
        state->machine_x,
        state->machine_y,
        state->machine_z,
        state->machine_x - state->work_x,
        state->machine_y - state->work_y,
        state->machine_z - state->work_z);
#if MOTION_BACKEND_DRV8835
    if (position_sensors_status_enabled()) {
        position_sensor_snapshot_t joint1;
        position_sensor_snapshot_t joint2;
        position_sensors_get_snapshot(0u, &joint1);
        position_sensors_get_snapshot(1u, &joint2);
        const unsigned int present_mask =
            (joint1.present ? 1u : 0u) | (joint2.present ? 2u : 0u);
        const unsigned int magnet_mask =
            (joint1.magnet_detected ? 1u : 0u) |
            (joint2.magnet_detected ? 2u : 0u);
        printf("|AS5600:%u,%u,%u,%u,%.3f,%.3f", present_mask,
               magnet_mask, joint1.raw, joint2.raw,
               (double)joint1.degrees, (double)joint2.degrees);
    }
#endif
    printf(">\r\n");
}

static void ok(void) {
    printf("ok\r\n");
}

static void error(const char *message) {
    printf("error: %s\r\n", message);
}

static void handle_motion(const char *line, controller_state_t *state, bool rapid) {
    float x = state->work_x;
    float y = state->work_y;
    float z = state->work_z;
    float value = 0.0f;
    const bool has_x = word_value(line, 'X', &value);
    if (has_x) {
        x = state->absolute ? value : state->work_x + value;
    }
    const bool has_y = word_value(line, 'Y', &value);
    if (has_y) {
        y = state->absolute ? value : state->work_y + value;
    }
    const bool has_z = word_value(line, 'Z', &value);
    if (has_z) {
        z = state->absolute ? value : state->work_z + value;
    }
#if !MOTION_BACKEND_DRV8835
    (void)z;
    (void)has_z;
#endif
    if (word_value(line, 'F', &value) && value > 0.0f) {
        state->feed_mm_min = value;
    }
#if MOTION_BACKEND_DRV8835
    if (has_z) {
        bool changed = false;
        if (!servo_pen_set_z(z, &changed)) {
            error("servo Z command failed");
            return;
        }
        state->work_z = z;
        state->machine_z = z;
        state->pen_pwm = (int)servo_pen_current_pulse_us();
        state->pen_up = servo_pen_is_up();
        if (changed) {
            servo_pen_config_t config;
            servo_pen_get_config(&config);
            planner_dwell_ms(config.settle_ms);
        }
        printf("[MSG:Z %.3f pen=%s pulse_us=%u pin=GP%u]\r\n",
               (double)z, state->pen_up ? "up" : "down",
               servo_pen_current_pulse_us(), PIN_PEN_SERVO_PWM);
    }
#endif
    if ((has_x || has_y) &&
        !planner_line_to(state, x, y, state->feed_mm_min, rapid)) {
#if MOTION_BACKEND_DRV8835
        printf("[MSG:motion blocked; use M17 before motion]\r\n");
#else
        printf("[MSG:motion blocked; TMC UART initialization not ready, use M122]\r\n");
#endif
    }
    ok();
}

static void handle_jog(const char *line, controller_state_t *state) {
    float dx = 0.0f;
    float dy = 0.0f;
    float feed = DEFAULT_JOG_FEED_MM_MIN;
    float value = 0.0f;
    if (word_value(line, 'X', &value)) {
        dx = value;
    }
    if (word_value(line, 'Y', &value)) {
        dy = value;
    }
    if (word_value(line, 'F', &value) && value > 0.0f) {
        feed = value;
    }
    if (!planner_jog(state, dx, dy, feed)) {
#if MOTION_BACKEND_DRV8835
        printf("[MSG:jog blocked; use M17 before motion]\r\n");
#else
        printf("[MSG:jog blocked; TMC UART initialization not ready, use M122]\r\n");
#endif
    }
    ok();
}

static void handle_g10(const char *line, controller_state_t *state) {
    float value = 0.0f;
    if (strstr(line, "L20") == NULL) {
        error("only G10 L20 is supported");
        return;
    }
    if (word_value(line, 'X', &value)) {
        state->work_x = value;
    }
    if (word_value(line, 'Y', &value)) {
        state->work_y = value;
    }
    if (word_value(line, 'Z', &value)) {
        state->work_z = value;
    }
    state->machine_x = state->work_x;
    state->machine_y = state->work_y;
    state->machine_z = state->work_z;
    planner_reset_position(state);
    ok();
}

static void handle_mcode(const char *line, controller_state_t *state) {
    if (has_prefix(line, "M3")) {
        float value = 0.0f;
        if (word_value(line, 'S', &value)) {
#if MOTION_BACKEND_DRV8835
            const long pulse_us = lroundf(value);
            if (pulse_us < (long)SERVO_PEN_MIN_PULSE_US ||
                pulse_us > (long)SERVO_PEN_MAX_PULSE_US ||
                fabsf(value - (float)pulse_us) >= 0.001f ||
                !servo_pen_set_pulse_us((unsigned int)pulse_us)) {
                error("M3 S requires servo pulse 500..2500us");
                return;
            }
            servo_pen_config_t config;
            servo_pen_get_config(&config);
            planner_dwell_ms(config.settle_ms);
            state->pen_up = servo_pen_is_up();
            if ((unsigned int)pulse_us == config.up_pulse_us) {
                state->work_z = 1.0f;
                state->machine_z = 1.0f;
            } else if ((unsigned int)pulse_us == config.down_pulse_us) {
                state->work_z = 0.0f;
                state->machine_z = 0.0f;
            }
            printf("[MSG:M3 servo pulse_us=%ld pin=GP%u]\r\n",
                   pulse_us, PIN_PEN_SERVO_PWM);
#endif
            state->pen_pwm = (int)value;
        }
        ok();
        return;
    }
#if MOTION_BACKEND_DRV8835
    if (has_prefix(line, "M5")) {
        bool changed = false;
        servo_pen_move_up(&changed);
        state->pen_pwm = (int)servo_pen_current_pulse_us();
        state->pen_up = true;
        state->work_z = 1.0f;
        state->machine_z = 1.0f;
        if (changed) {
            servo_pen_config_t config;
            servo_pen_get_config(&config);
            planner_dwell_ms(config.settle_ms);
        }
        printf("[MSG:M5 pen=up pulse_us=%u pin=GP%u]\r\n",
               servo_pen_current_pulse_us(), PIN_PEN_SERVO_PWM);
        ok();
        return;
    }
#endif
    if (has_prefix(line, "M17")) {
        state->enabled = planner_enable(true);
        state->alarm = !state->enabled;
#if MOTION_BACKEND_DRV8835
        printf("[ENABLE M17 result=%s backend=DRV8835 outputs=HiZ]\r\n",
               state->enabled ? "armed" : "blocked");
#else
        printf("[ENABLE M17 result=%s X_EN=%u Y_EN=%u]\r\n", state->enabled ? "enabled" : "blocked",
               gpio_get(PIN_X_ENABLE), gpio_get(PIN_Y_ENABLE));
#endif
        ok();
        return;
    }
    if (has_prefix(line, "M18") || has_prefix(line, "M84")) {
        state->enabled = false;
        planner_enable(false);
#if MOTION_BACKEND_DRV8835
        servo_pen_move_up(NULL);
        state->pen_pwm = (int)servo_pen_current_pulse_us();
        state->pen_up = true;
        state->work_z = 1.0f;
        state->machine_z = 1.0f;
        printf("[ENABLE M18 result=disabled backend=DRV8835 outputs=HiZ]\r\n");
#else
        printf("[ENABLE M18 result=disabled X_EN=%u Y_EN=%u]\r\n",
               gpio_get(PIN_X_ENABLE), gpio_get(PIN_Y_ENABLE));
#endif
        ok();
        return;
    }
    if (has_prefix(line, "M114")) {
        printf("WPos:%.3f,%.3f,%.3f\r\n",
               state->work_x, state->work_y, state->work_z);
        ok();
        return;
    }
    if (has_prefix(line, "M115")) {
        printf("FIRMWARE_NAME:%s FIRMWARE_VERSION:%s PROTOCOL_VERSION:1.1 "
               "MACHINE:Pico2_XY_Planar CURRENT_MONITOR:M982 "
               "POSITION_SENSORS:M983\r\n",
               FW_NAME, FW_VERSION);
        ok();
        return;
    }
    if (has_prefix(line, "M122")) {
#if MOTION_BACKEND_DRV8835
        stepper_drv8835_report_status();
        servo_pen_config_t servo_config;
        servo_pen_get_config(&servo_config);
        printf("[MSG:SERVO pin=GP%u frequency_hz=%u pulse_us=%u state=%s "
               "up_us=%u down_us=%u settle_ms=%u z_threshold=%.3f]\r\n",
               PIN_PEN_SERVO_PWM, SERVO_PEN_PWM_FREQUENCY_HZ,
               servo_pen_current_pulse_us(),
               servo_pen_is_up() ? "up" : "down",
               servo_config.up_pulse_us, servo_config.down_pulse_us,
               servo_config.settle_ms, (double)servo_config.z_threshold);
        current_monitor_report();
        position_sensors_report();
#else
        const bool plotterflow_debug = strchr(line + 4, 'P') != NULL;
        const bool select_x = strchr(line + 4, 'X') != NULL;
        const bool select_y = strchr(line + 4, 'Y') != NULL;
        uint8_t axes = TMC_AXIS_ALL;
        if (select_x || select_y) {
            axes = (select_x ? TMC_AXIS_X : 0u) | (select_y ? TMC_AXIS_Y : 0u);
        }
        if (plotterflow_debug) {
            tmc2209_report_plotterflow_debug(axes);
        } else {
            tmc2209_report_status(axes);
        }
#endif
        ok();
        return;
    }
#if MOTION_BACKEND_DRV8835
    if (has_prefix(line, "M982")) {
        float zero = 0.0f;
        if (word_value(line, 'Z', &zero)) {
            if (!isfinite(zero) || fabsf(zero - 1.0f) > 0.001f) {
                error("M982 accepts Z1 for zero calibration");
                return;
            }
            if (stepper_is_enabled() || stepper_drv8835_outputs_active()) {
                error("M982 Z1 requires M18 and HiZ outputs");
                return;
            }
            const bool present = current_monitor_calibrate();
            printf("[MSG:M982 zero=%s reason=%s]\r\n",
                   present ? "ok" : "skipped",
                   present ? "sensor-present" : "sensor-not-present");
        }
        current_monitor_report();
        ok();
        return;
    }
    if (has_prefix(line, "M983")) {
        float enabled = 0.0f;
        if (word_value(line, 'S', &enabled)) {
            if (!isfinite(enabled) ||
                (fabsf(enabled) > 0.001f && fabsf(enabled - 1.0f) > 0.001f)) {
                error("M983 S must be 0 or 1");
                return;
            }
            position_sensors_set_status_enabled(enabled > 0.5f);
        }
        position_sensors_refresh();
        position_sensors_report();
        ok();
        return;
    }
    if (has_prefix(line, "M281")) {
        servo_pen_config_t config;
        servo_pen_get_config(&config);
        float value = 0.0f;
        bool has_setting = false;
        bool valid = true;
        if (word_value(line, 'U', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= (long)SERVO_PEN_MIN_PULSE_US &&
                    rounded <= (long)SERVO_PEN_MAX_PULSE_US &&
                    fabsf(value - (float)rounded) < 0.001f;
            config.up_pulse_us = rounded > 0 ? (unsigned int)rounded : 0u;
            has_setting = true;
        }
        if (word_value(line, 'D', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= (long)SERVO_PEN_MIN_PULSE_US &&
                    rounded <= (long)SERVO_PEN_MAX_PULSE_US &&
                    fabsf(value - (float)rounded) < 0.001f;
            config.down_pulse_us = rounded > 0 ? (unsigned int)rounded : 0u;
            has_setting = true;
        }
        if (word_value(line, 'T', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 0 &&
                    rounded <= (long)SERVO_PEN_MAX_SETTLE_MS &&
                    fabsf(value - (float)rounded) < 0.001f;
            config.settle_ms =
                rounded >= 0 ? (unsigned int)rounded
                             : SERVO_PEN_MAX_SETTLE_MS + 1u;
            has_setting = true;
        }
        if (word_value(line, 'Z', &value)) {
            valid = valid && isfinite(value);
            config.z_threshold = value;
            has_setting = true;
        }
        if (has_setting && (!valid || !servo_pen_configure(&config))) {
            error("M281 requires U/D500..2500 T0..2000 and finite Z threshold");
            return;
        }
        servo_pen_get_config(&config);
        state->pen_pwm = (int)servo_pen_current_pulse_us();
        state->pen_up = servo_pen_is_up();
        printf("[MSG:M281 servo=PWM pin=GP%u frequency_hz=%u "
               "up_us=%u down_us=%u settle_ms=%u z_threshold=%.3f "
               "current_us=%u state=%s]\r\n",
               PIN_PEN_SERVO_PWM, SERVO_PEN_PWM_FREQUENCY_HZ,
               config.up_pulse_us, config.down_pulse_us, config.settle_ms,
               (double)config.z_threshold, servo_pen_current_pulse_us(),
               servo_pen_is_up() ? "up" : "down");
        ok();
        return;
    }
    if (has_prefix(line, "M980")) {
        unsigned int microsteps = 0u;
        unsigned int x_run_peak = 0u;
        unsigned int y_run_peak = 0u;
        unsigned int x_start_peak = 0u;
        unsigned int y_start_peak = 0u;
        unsigned int boost_steps = 0u;
        unsigned int boost_ramp_steps = 0u;
        unsigned int hold_ms = 0u;
        unsigned int capture_ms = 0u;
        bool active_axes_only = false;
        stepper_drv8835_get_config(
            &microsteps, &x_run_peak, &y_run_peak, &x_start_peak,
            &y_start_peak, &boost_steps, &boost_ramp_steps, &hold_ms,
            &capture_ms, &active_axes_only);

        float value = 0.0f;
        bool has_setting = false;
        bool valid = true;
        if (word_value(line, 'U', &value)) {
            const long rounded = lroundf(value);
            valid = rounded > 0 && fabsf(value - (float)rounded) < 0.001f;
            microsteps = valid ? (unsigned int)rounded : 0u;
            has_setting = true;
        }
        if (word_value(line, 'X', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 1 && rounded <= 100 &&
                    fabsf(value - (float)rounded) < 0.001f;
            x_run_peak = rounded > 0 ? (unsigned int)rounded : 0u;
            has_setting = true;
        }
        if (word_value(line, 'Y', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 1 && rounded <= 100 &&
                    fabsf(value - (float)rounded) < 0.001f;
            y_run_peak = rounded > 0 ? (unsigned int)rounded : 0u;
            has_setting = true;
        }
        if (word_value(line, 'I', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 1 && rounded <= 100 &&
                    fabsf(value - (float)rounded) < 0.001f;
            x_start_peak = rounded > 0 ? (unsigned int)rounded : 0u;
            has_setting = true;
        }
        if (word_value(line, 'J', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 1 && rounded <= 100 &&
                    fabsf(value - (float)rounded) < 0.001f;
            y_start_peak = rounded > 0 ? (unsigned int)rounded : 0u;
            has_setting = true;
        }
        if (word_value(line, 'B', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 0 &&
                    rounded <= (long)DRV8835_MAX_BOOST_STEPS &&
                    fabsf(value - (float)rounded) < 0.001f;
            boost_steps = rounded >= 0 ? (unsigned int)rounded
                                       : DRV8835_MAX_BOOST_STEPS + 1u;
            has_setting = true;
        }
        if (word_value(line, 'R', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 0 &&
                    rounded <= (long)DRV8835_MAX_BOOST_STEPS &&
                    fabsf(value - (float)rounded) < 0.001f;
            boost_ramp_steps = rounded >= 0 ? (unsigned int)rounded
                                            : DRV8835_MAX_BOOST_STEPS + 1u;
            has_setting = true;
        }
        if (word_value(line, 'H', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 0 && rounded <= 5000 &&
                    fabsf(value - (float)rounded) < 0.001f;
            hold_ms = rounded >= 0 ? (unsigned int)rounded : 5001u;
            has_setting = true;
        }
        if (word_value(line, 'A', &value)) {
            const long rounded = lroundf(value);
            valid = valid && (rounded == 0 || rounded == 1) &&
                    fabsf(value - (float)rounded) < 0.001f;
            active_axes_only = rounded == 1;
            has_setting = true;
        }
        if (word_value(line, 'C', &value)) {
            const long rounded = lroundf(value);
            valid = valid && rounded >= 0 && rounded <= 500 &&
                    fabsf(value - (float)rounded) < 0.001f;
            capture_ms = rounded >= 0 ? (unsigned int)rounded : 501u;
            has_setting = true;
        }
        if (has_setting && (!valid ||
            !stepper_drv8835_configure(
                microsteps, x_run_peak, y_run_peak, x_start_peak,
                y_start_peak, boost_steps, boost_ramp_steps, hold_ms,
                capture_ms, active_axes_only))) {
            error("M980 requires M18 and U1|2|4|8|16 X/Y/I/J1..100 B/R0..10000 H0..5000 C0..500 A0|1");
            return;
        }
        stepper_drv8835_get_config(
            &microsteps, &x_run_peak, &y_run_peak, &x_start_peak,
            &y_start_peak, &boost_steps, &boost_ramp_steps, &hold_ms,
            &capture_ms, &active_axes_only);
        const bool boost_enabled =
            drv8835_boost_enabled(x_run_peak, x_start_peak, boost_steps,
                                  boost_ramp_steps) ||
            drv8835_boost_enabled(y_run_peak, y_start_peak, boost_steps,
                                  boost_ramp_steps);
        printf("[MSG:M980 backend=DRV8835 microsteps=%u step_mm=%.5f "
               "drive_mode=%s X_peak=%u Y_peak=%u X_run=%u Y_run=%u "
               "X_start=%u Y_start=%u "
               "boost_steps=%u boost_ramp_steps=%u boost_enabled=%u "
               "hold_ms=%u capture_ms=%u axis_mode=%s]\r\n",
               microsteps, (double)(1.0f / (DEFAULT_STEPS_PER_MM * microsteps)),
               microsteps == 1u ? "single-phase" : "microstep",
               x_run_peak, y_run_peak,
               x_run_peak, y_run_peak, x_start_peak, y_start_peak,
               boost_steps, boost_ramp_steps, boost_enabled ? 1u : 0u,
               hold_ms, capture_ms,
               active_axes_only ? "moving-only" : "both");
        ok();
        return;
    }
#else
    if (has_prefix(line, "M906")) {
        float x_current = 0.0f;
        float y_current = 0.0f;
        float all_current = 0.0f;
        float hold_current = 0.0f;
        bool set_x = word_value(line, 'X', &x_current);
        bool set_y = word_value(line, 'Y', &y_current);
        const bool set_all = word_value(line, 'I', &all_current);
        const bool set_hold = word_value(line, 'H', &hold_current);
        if (set_all) {
            x_current = all_current;
            y_current = all_current;
            set_x = true;
            set_y = true;
        }
        bool success = set_x || set_y;
        if ((set_x && (x_current < TMC_MIN_CURRENT_MA || x_current > TMC_MAX_CURRENT_MA)) ||
            (set_y && (y_current < TMC_MIN_CURRENT_MA || y_current > TMC_MAX_CURRENT_MA)) ||
            (set_hold && (hold_current < TMC_MIN_CURRENT_MA || hold_current > TMC_MAX_CURRENT_MA))) {
            printf("[TMC M906 rejected: current range is %u..%umA]\r\n",
                   TMC_MIN_CURRENT_MA, TMC_MAX_CURRENT_MA);
            success = false;
        } else {
            if (set_x) {
                const uint16_t run_ma = (uint16_t)x_current;
                const uint16_t requested_hold = set_hold ? (uint16_t)hold_current : DEFAULT_X_HOLD_CURRENT_MA;
                const uint16_t hold_ma = run_ma < requested_hold ? run_ma : requested_hold;
                success = tmc2209_set_current(TMC_ADDR_X, run_ma, hold_ma) && success;
            }
            if (set_y) {
                const uint16_t run_ma = (uint16_t)y_current;
                const uint16_t requested_hold = set_hold ? (uint16_t)hold_current : DEFAULT_Y_HOLD_CURRENT_MA;
                const uint16_t hold_ma = run_ma < requested_hold ? run_ma : requested_hold;
                success = tmc2209_set_current(TMC_ADDR_Y, run_ma, hold_ma) && success;
            }
        }
        if (!success) {
            state->enabled = false;
            planner_enable(false);
            printf("[TMC M906 failed or missing axis/current; drivers disabled]\r\n");
        } else if (tmc2209_all_ready()) {
            state->alarm = false;
        }
        ok();
        return;
    }
    if (has_prefix(line, "M970")) {
        float x_steps = 0.0f;
        float y_steps = 0.0f;
        float dwell_ms = 250.0f;
        const bool use_x = word_value(line, 'X', &x_steps);
        const bool use_y = word_value(line, 'Y', &y_steps);
        (void)word_value(line, 'P', &dwell_ms);
        const float requested_steps = use_x ? x_steps : y_steps;
        const unsigned int steps = (unsigned int)lroundf(fabsf(requested_steps));
        if (use_x == use_y || steps < 1u || steps > 64u || dwell_ms < 10.0f || dwell_ms > 2000.0f) {
            error("M970 requires one axis X/Y with 1..64 steps and P10..2000ms");
            return;
        }
        planner_enable(false);
        if (!ensure_diagnostic_drivers_ready()) {
            error("M970 blocked; TMC reinitialization failed");
            return;
        }
        const char axis = use_x ? 'X' : 'Y';
        state->enabled = enable_diagnostic_axis(axis);
        if (!state->enabled) {
            error("M970 blocked; TMC UART initialization not ready");
            return;
        }
        const uint8_t address = use_x ? TMC_ADDR_X : TMC_ADDR_Y;
        const bool positive = requested_steps >= 0.0f;
        bool success = tmc2209_report_phase(address, 0);
        for (unsigned int i = 0; i < steps; i++) {
            success = stepper_diagnostic_step(axis, positive) && success;
            planner_dwell_ms((unsigned int)dwell_ms);
            success = tmc2209_report_phase(address, i + 1u) && success;
        }
        stepper_get_position(&state->machine_x, &state->machine_y);
        state->work_x = state->machine_x;
        state->work_y = state->machine_y;
        state->enabled = false;
        planner_enable(false);
        printf("[MSG:M970 axis=%c steps=%u direction=%c dwell_ms=%u result=%s EN=disabled]\r\n",
               axis, steps, positive ? '+' : '-', (unsigned int)dwell_ms,
               success ? "ok" : "error");
        ok();
        return;
    }
    if (has_prefix(line, "M971")) {
        float x_cycles = 0.0f;
        float y_cycles = 0.0f;
        float dwell_ms = 100.0f;
        const bool use_x = word_value(line, 'X', &x_cycles);
        const bool use_y = word_value(line, 'Y', &y_cycles);
        (void)word_value(line, 'P', &dwell_ms);
        const float requested_cycles = use_x ? x_cycles : y_cycles;
        const unsigned int cycles = (unsigned int)lroundf(fabsf(requested_cycles));
        if (use_x == use_y || cycles < 1u || cycles > 8u || dwell_ms < 10.0f || dwell_ms > 1000.0f) {
            error("M971 requires one axis X/Y with 1..8 cycles and P10..1000ms");
            return;
        }

        const char axis = use_x ? 'X' : 'Y';
        const uint8_t address = use_x ? TMC_ADDR_X : TMC_ADDR_Y;
        const bool positive = requested_cycles >= 0.0f;
        const uint16_t original_microsteps = tmc2209_get_microsteps(address);
        // TMC2209 decrements MSCNT while DIR is high (the default positive direction).
        const uint16_t forward_targets[4] = {0u, 768u, 512u, 256u};
        const uint16_t reverse_targets[4] = {0u, 256u, 512u, 768u};
        const uint16_t *targets = positive ? forward_targets : reverse_targets;
        bool success = original_microsteps != 0u;

        planner_enable(false);
        success = ensure_diagnostic_drivers_ready() && success;
        success = tmc2209_set_microsteps(address, 256u) && success;
        if (success) {
            state->enabled = enable_diagnostic_axis(axis);
            success = state->enabled;
        }

        unsigned int state_index = 0u;
        for (unsigned int cycle = 0u; success && cycle < cycles; cycle++) {
            for (unsigned int phase = 0u; success && phase < 4u; phase++) {
                uint16_t current = 0u;
                success = tmc2209_get_mscnt(address, &current);
                const uint16_t target = targets[phase];
                const unsigned int pulses = positive
                    ? (unsigned int)((current - target) & 0x3FFu)
                    : (unsigned int)((target - current) & 0x3FFu);
                success = stepper_diagnostic_pulses(axis, positive, pulses, 20u) && success;
                success = tmc2209_report_phase(address, state_index++) && success;
                if (success) {
                    planner_dwell_ms((unsigned int)dwell_ms);
                }
            }
        }

        state->enabled = false;
        planner_enable(false);
        const bool restored = original_microsteps != 0u &&
                              tmc2209_set_microsteps(address, original_microsteps);
        success = restored && success;
        printf("[MSG:M971 axis=%c cycles=%u direction=%c dwell_ms=%u mode=single-phase result=%s EN=disabled restored_microsteps=%u]\r\n",
               axis, cycles, positive ? '+' : '-', (unsigned int)dwell_ms,
               success ? "ok" : "error", restored ? original_microsteps : 0u);
        ok();
        return;
    }
    if (has_prefix(line, "M972")) {
        float x_cycles = 0.0f;
        float y_cycles = 0.0f;
        float phase_value = 0.0f;
        float dwell_ms = 500.0f;
        const bool use_x = word_value(line, 'X', &x_cycles);
        const bool use_y = word_value(line, 'Y', &y_cycles);
        (void)word_value(line, 'S', &phase_value);
        (void)word_value(line, 'P', &dwell_ms);
        const float requested_cycles = use_x ? x_cycles : y_cycles;
        const unsigned int cycles = (unsigned int)lroundf(fabsf(requested_cycles));
        const unsigned int phase = (unsigned int)lroundf(phase_value);
        if (use_x == use_y || cycles < 1u || cycles > 10u || phase > 3u ||
            dwell_ms < 50.0f || dwell_ms > 2000.0f) {
            error("M972 requires one axis X/Y with 1..10 cycles, S0..3, and P50..2000ms");
            return;
        }

        const char axis = use_x ? 'X' : 'Y';
        const uint8_t address = use_x ? TMC_ADDR_X : TMC_ADDR_Y;
        const uint16_t original_microsteps = tmc2209_get_microsteps(address);
        const uint16_t targets[4] = {0u, 768u, 512u, 256u};
        const char *phase_names[4] = {"A+", "B-", "A-", "B+"};
        bool success = original_microsteps != 0u;

        state->enabled = false;
        planner_enable(false);
        success = ensure_diagnostic_drivers_ready() && success;
        success = tmc2209_set_microsteps(address, 256u) && success;
        if (success) {
            state->enabled = enable_diagnostic_axis(axis);
            success = state->enabled;
        }
        if (success) {
            uint16_t current = 0u;
            success = tmc2209_get_mscnt(address, &current);
            const unsigned int pulses = (unsigned int)((current - targets[phase]) & 0x3FFu);
            success = stepper_diagnostic_pulses(axis, true, pulses, 20u) && success;
            success = tmc2209_report_phase(address, phase) && success;
        }

        for (unsigned int cycle = 0u; success && cycle < cycles; cycle++) {
            if (cycle > 0u) {
                state->enabled = enable_diagnostic_axis(axis);
                success = state->enabled;
            }
            if (!success) {
                break;
            }
            printf("[MSG:M972 axis=%c phase=%s cycle=%u/%u state=ON dwell_ms=%u EN=enabled]\r\n",
                   axis, phase_names[phase], cycle + 1u, cycles, (unsigned int)dwell_ms);
            const unsigned int probe_delay_ms = dwell_ms < 50.0f ? (unsigned int)dwell_ms : 50u;
            planner_dwell_ms(probe_delay_ms);
            success = tmc2209_report_live_output(address, "M972_ON") && success;
            if (success && (unsigned int)dwell_ms > probe_delay_ms) {
                planner_dwell_ms((unsigned int)dwell_ms - probe_delay_ms);
            }
            state->enabled = false;
            planner_enable(false);
            printf("[MSG:M972 axis=%c phase=%s cycle=%u/%u state=OFF dwell_ms=%u EN=disabled]\r\n",
                   axis, phase_names[phase], cycle + 1u, cycles, (unsigned int)dwell_ms);
            planner_dwell_ms((unsigned int)dwell_ms);
        }

        state->enabled = false;
        planner_enable(false);
        const bool restored = original_microsteps != 0u &&
                              tmc2209_set_microsteps(address, original_microsteps);
        success = restored && success;
        printf("[MSG:M972 axis=%c phase=%s cycles=%u result=%s EN=disabled restored_microsteps=%u]\r\n",
               axis, phase_names[phase], cycles, success ? "ok" : "error",
               restored ? original_microsteps : 0u);
        ok();
        return;
    }
    if (has_prefix(line, "M973")) {
        float profile_value = 0.0f;
        float phase_value = 0.0f;
        float current_value = 0.0f;
        float dwell_value = 0.0f;
        const bool use_x = strchr(line + 4, 'X') != NULL;
        const bool use_y = strchr(line + 4, 'Y') != NULL;
        const bool has_profile = word_value(line, 'Q', &profile_value);
        const bool has_phase = word_value(line, 'S', &phase_value);
        const bool has_current = word_value(line, 'I', &current_value);
        const bool has_dwell = word_value(line, 'P', &dwell_value);
        const unsigned int profile = (unsigned int)lroundf(profile_value);
        const unsigned int phase = (unsigned int)lroundf(phase_value);
        const unsigned int current_ma = (unsigned int)lroundf(current_value);
        const unsigned int dwell_ms = (unsigned int)lroundf(dwell_value);
        const bool integers = fabsf(profile_value - (float)profile) < 0.001f &&
                              fabsf(phase_value - (float)phase) < 0.001f &&
                              fabsf(current_value - (float)current_ma) < 0.001f &&
                              fabsf(dwell_value - (float)dwell_ms) < 0.001f;
        if (use_x == use_y || !has_profile || !has_phase || !has_current || !has_dwell ||
            !integers || profile > 3u || phase > 3u ||
            current_ma < TMC_MIN_CURRENT_MA || current_ma > TMC_MAX_CURRENT_MA ||
            dwell_ms < 50u || dwell_ms > 2000u) {
            error("M973 requires one bare axis X/Y, Q0..3, S0..3, I50..2000, and P50..2000ms");
            return;
        }

        const char axis = use_x ? 'X' : 'Y';
        const uint8_t address = use_x ? TMC_ADDR_X : TMC_ADDR_Y;
        const uint16_t targets[4] = {0u, 768u, 512u, 256u};
        const char *phase_names[4] = {"A+", "B-", "A-", "B+"};
        bool success = false;

        state->enabled = false;
        planner_enable(false);
        if (tmc2209_ensure_ready(address)) {
            success = tmc2209_begin_chopper_test(address, (uint8_t)profile,
                                                 (uint16_t)current_ma);
        }
        if (success) {
            state->enabled = stepper_enable_axis(axis, true);
            tmc2209_set_enabled(state->enabled);
            success = state->enabled;
        }
        if (success) {
            uint16_t current = 0u;
            success = tmc2209_get_mscnt(address, &current);
            const unsigned int pulses = (unsigned int)((current - targets[phase]) & 0x3FFu);
            success = stepper_diagnostic_pulses(axis, true, pulses, 20u) && success;
            success = tmc2209_report_phase(address, phase) && success;
        }
        if (success) {
            printf("[MSG:M973 axis=%c profile=Q%u phase=%s current=%umA state=ON "
                   "dwell_ms=%u EN=enabled]\r\n",
                   axis, profile, phase_names[phase], current_ma, dwell_ms);
            const unsigned int probe_delay_ms = dwell_ms < 50u ? dwell_ms : 50u;
            planner_dwell_ms(probe_delay_ms);
            success = tmc2209_report_live_output(address, "M973_ON") && success;
            if (success && dwell_ms > probe_delay_ms) {
                planner_dwell_ms(dwell_ms - probe_delay_ms);
            }
        }

        state->enabled = false;
        planner_enable(false);
        printf("[MSG:M973 axis=%c profile=Q%u phase=%s state=OFF EN=disabled]\r\n",
               axis, profile, phase_names[phase]);
        const bool restored = tmc2209_end_chopper_test(address);
        success = restored && success;
        if (!success) {
            state->alarm = true;
        }
        printf("[MSG:M973 axis=%c profile=Q%u phase=%s current=%umA dwell_ms=%u "
               "result=%s EN=disabled restored=%s]\r\n",
               axis, profile, phase_names[phase], current_ma, dwell_ms,
               success ? "ok" : "error", restored ? "yes" : "no");
        ok();
        return;
    }
#endif
    if (has_prefix(line, "M569")) {
        printf("[MSG:M569 parsed as compatibility no-op]\r\n");
        ok();
        return;
    }
    error("unsupported M-code");
}

void gcode_process_line(char *line, controller_state_t *state) {
    strip_comments(line);
    char *clean = trim(line);
    if (*clean == '\0') {
        ok();
        return;
    }
    if (clean[0] == '$') {
        if (has_prefix(clean, "$J=")) {
            handle_jog(clean + 3, state);
        } else {
            printf("[MSG:unsupported setting command]\r\n");
            ok();
        }
        return;
    }
    if (has_prefix(clean, "G0") || has_prefix(clean, "G00")) {
        handle_motion(clean, state, true);
        return;
    }
    if (has_prefix(clean, "G1") && !has_prefix(clean, "G10")) {
        handle_motion(clean, state, false);
        return;
    }
    if (has_prefix(clean, "G4")) {
        float p = 0.0f;
        float s = 0.0f;
        if (word_value(clean, 'P', &p)) {
            planner_dwell_ms((unsigned int)fmaxf(0.0f, p));
        } else if (word_value(clean, 'S', &s)) {
            planner_dwell_ms((unsigned int)fmaxf(0.0f, s * 1000.0f));
        }
        ok();
        return;
    }
    if (has_prefix(clean, "G10")) {
        handle_g10(clean, state);
        return;
    }
    if (has_prefix(clean, "G20")) {
        error("inch mode is not supported");
        return;
    }
    if (has_prefix(clean, "G21")) {
        ok();
        return;
    }
    if (has_prefix(clean, "G90")) {
        state->absolute = true;
        ok();
        return;
    }
    if (has_prefix(clean, "G91")) {
        state->absolute = false;
        ok();
        return;
    }
    if (has_prefix(clean, "G92")) {
        float value = 0.0f;
        if (word_value(clean, 'X', &value)) {
            state->work_x = value;
        }
        if (word_value(clean, 'Y', &value)) {
            state->work_y = value;
        }
        if (word_value(clean, 'Z', &value)) {
            state->work_z = value;
        }
        planner_reset_position(state);
        ok();
        return;
    }
    if (clean[0] == 'M') {
        handle_mcode(clean, state);
        return;
    }
    error("unsupported command");
}

void gcode_process_realtime(int ch, controller_state_t *state) {
    switch (ch) {
    case '?':
        gcode_report_status(state);
        break;
    case '!':
        planner_feed_hold(state);
        break;
    case '~':
        planner_resume(state);
        break;
    case 0x18:
        planner_soft_reset(state);
#if MOTION_BACKEND_DRV8835
        servo_pen_move_up(NULL);
        state->pen_pwm = (int)servo_pen_current_pulse_us();
        state->pen_up = true;
        state->work_z = 1.0f;
        state->machine_z = 1.0f;
#endif
        printf("\r\nGrbl 1.1h ['$' for help]\r\n");
        break;
    case 0x85:
        planner_cancel_jog(state);
#if MOTION_BACKEND_DRV8835
        servo_pen_move_up(NULL);
        state->pen_pwm = (int)servo_pen_current_pulse_us();
        state->pen_up = true;
        state->work_z = 1.0f;
        state->machine_z = 1.0f;
#endif
        break;
    default:
        break;
    }
}
