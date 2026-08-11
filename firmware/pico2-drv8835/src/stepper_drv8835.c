#include "stepper.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#include "config.h"
#include "drv8835_motion_math.h"

#define PWM_COUNTER_HZ 10000000u
#define PWM_WRAP ((PWM_COUNTER_HZ / DRV8835_PWM_FREQUENCY_HZ) - 1u)

enum {
    AXIS_X = 0,
    AXIS_Y = 1,
    AXIS_COUNT = 2,
};

static const unsigned int positive_pins[AXIS_COUNT][2] = {
    {PIN_DRV_X_A_IN1, PIN_DRV_X_B_IN1},
    {PIN_DRV_Y_C_IN1, PIN_DRV_Y_D_IN1},
};

static const unsigned int negative_pins[AXIS_COUNT][2] = {
    {PIN_DRV_X_A_IN2, PIN_DRV_X_B_IN2},
    {PIN_DRV_Y_C_IN2, PIN_DRV_Y_D_IN2},
};

static const unsigned int driver_pins[] = {
    PIN_DRV_X_A_IN1, PIN_DRV_X_A_IN2, PIN_DRV_X_B_IN1, PIN_DRV_X_B_IN2,
    PIN_DRV_Y_C_IN1, PIN_DRV_Y_C_IN2, PIN_DRV_Y_D_IN1, PIN_DRV_Y_D_IN2,
};

static unsigned int microsteps = DRV8835_DEFAULT_MICROSTEPS;
static unsigned int run_peak_duty[AXIS_COUNT] = {
    DRV8835_DEFAULT_X_PEAK_DUTY,
    DRV8835_DEFAULT_Y_PEAK_DUTY,
};
static unsigned int start_peak_duty[AXIS_COUNT] = {
    DRV8835_DEFAULT_X_START_PEAK_DUTY,
    DRV8835_DEFAULT_Y_START_PEAK_DUTY,
};
static unsigned int applied_peak_duty[AXIS_COUNT] = {
    DRV8835_DEFAULT_X_PEAK_DUTY,
    DRV8835_DEFAULT_Y_PEAK_DUTY,
};
static unsigned int boost_steps = DRV8835_DEFAULT_BOOST_STEPS;
static unsigned int boost_ramp_steps = DRV8835_DEFAULT_BOOST_RAMP_STEPS;
static unsigned int boost_axis_step_index[AXIS_COUNT] = {0u, 0u};
static unsigned int boost_active_mask = 0u;
static unsigned int hold_ms = DRV8835_DEFAULT_HOLD_MS;
static unsigned int capture_ms = DRV8835_DEFAULT_CAPTURE_MS;
static bool active_axes_only = DRV8835_DEFAULT_ACTIVE_AXES_ONLY != 0;
static unsigned int active_axis_mask = 0u;
static bool armed = false;
static bool outputs_active = false;
static int angle[AXIS_COUNT] = {0, 0};
static int coil_duty[AXIS_COUNT][2] = {{0, 0}, {0, 0}};
static long physical_step[AXIS_COUNT] = {0, 0};
static float coordinate_origin[AXIS_COUNT] = {0.0f, 0.0f};
static float commanded_position[AXIS_COUNT] = {0.0f, 0.0f};
static absolute_time_t release_deadline;

static const char *phase_name(unsigned int axis, int electrical_angle) {
    static const char *const phase_names[AXIS_COUNT][4] = {
        {"A+", "B-", "A-", "B+"},
        {"C+", "D-", "C-", "D+"},
    };
    const unsigned int phase = (unsigned int)(
        drv8835_wrap_angle(electrical_angle + DRV8835_QUADRANT_UNITS / 2) /
        DRV8835_QUADRANT_UNITS) % 4u;
    return phase_names[axis][phase];
}

static float effective_steps_per_mm(void) {
    return DEFAULT_STEPS_PER_MM * (float)microsteps;
}

static void set_all_pins_low(void) {
    for (size_t i = 0; i < sizeof(driver_pins) / sizeof(driver_pins[0]); i++) {
        const unsigned int pin = driver_pins[i];
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
    }
}

static void all_off(void) {
    set_all_pins_low();
    outputs_active = false;
    coil_duty[AXIS_X][0] = 0;
    coil_duty[AXIS_X][1] = 0;
    coil_duty[AXIS_Y][0] = 0;
    coil_duty[AXIS_Y][1] = 0;
    active_axis_mask = 0u;
    boost_active_mask = 0u;
    for (unsigned int axis = 0u; axis < AXIS_COUNT; axis++) {
        boost_axis_step_index[axis] = 0u;
        applied_peak_duty[axis] = run_peak_duty[axis];
    }
}

static void set_pin_duty(unsigned int pin, unsigned int duty_percent) {
    if (duty_percent == 0u) {
        return;
    }
    if (duty_percent >= 100u) {
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 1);
        return;
    }
    const uint32_t level =
        ((uint32_t)(PWM_WRAP + 1u) * duty_percent + 99u) / 100u;
    pwm_set_gpio_level(pin, level);
    gpio_set_function(pin, GPIO_FUNC_PWM);
}

static void apply_axis(unsigned int axis) {
    drv8835_axis_duties(angle[axis], applied_peak_duty[axis],
                        &coil_duty[axis][0], &coil_duty[axis][1]);
    for (unsigned int coil = 0; coil < 2u; coil++) {
        const int duty = coil_duty[axis][coil];
        const unsigned int pin = duty >= 0 ? positive_pins[axis][coil]
                                           : negative_pins[axis][coil];
        set_pin_duty(pin, (unsigned int)abs(duty));
    }
}

static void prepare_motion_boost(unsigned int next_active_mask,
                                 unsigned int newly_energized_mask) {
    /* Preserve progress across consecutive blocks on an already held axis. */
    boost_active_mask &= next_active_mask;
    for (unsigned int axis = 0u; axis < AXIS_COUNT; axis++) {
        const unsigned int axis_bit = 1u << axis;
        applied_peak_duty[axis] = run_peak_duty[axis];
        if ((next_active_mask & axis_bit) == 0u) {
            boost_axis_step_index[axis] = 0u;
            boost_active_mask &= ~axis_bit;
        } else if ((newly_energized_mask & axis_bit) != 0u &&
            drv8835_boost_enabled(run_peak_duty[axis],
                                  start_peak_duty[axis], boost_steps,
                                  boost_ramp_steps)) {
            boost_axis_step_index[axis] = 0u;
            applied_peak_duty[axis] = start_peak_duty[axis];
            boost_active_mask |= axis_bit;
        } else if ((newly_energized_mask & axis_bit) != 0u) {
            boost_axis_step_index[axis] = 0u;
            boost_active_mask &= ~axis_bit;
        }
    }
}

static bool prepare_axis_step(unsigned int axis) {
    const unsigned int axis_bit = 1u << axis;
    if ((boost_active_mask & axis_bit) == 0u) {
        applied_peak_duty[axis] = run_peak_duty[axis];
        return false;
    }
    applied_peak_duty[axis] = drv8835_boost_duty_for_step(
        run_peak_duty[axis], start_peak_duty[axis], boost_steps,
        boost_ramp_steps, boost_axis_step_index[axis]);
    boost_axis_step_index[axis]++;
    return boost_axis_step_index[axis] >= boost_steps + boost_ramp_steps;
}

static void apply_outputs(void) {
    set_all_pins_low();
    for (unsigned int axis = 0; axis < AXIS_COUNT; axis++) {
        if ((active_axis_mask & (1u << axis)) != 0u) {
            apply_axis(axis);
        } else {
            coil_duty[axis][0] = 0;
            coil_duty[axis][1] = 0;
        }
    }
    outputs_active = active_axis_mask != 0u;
    sleep_us(10);
}

static void schedule_release(void) {
    if (hold_ms == 0u) {
        all_off();
    } else {
        release_deadline = make_timeout_time_ms(hold_ms);
    }
}

static void wait_motion_interval(unsigned int interval_us,
                                 volatile bool *cancelled,
                                 stepper_realtime_callback_t callback,
                                 void *context) {
    while (interval_us > 0u && (!cancelled || !*cancelled)) {
        if (callback) {
            callback(context);
        }
        const unsigned int slice_us = interval_us > 1000u ? 1000u : interval_us;
        sleep_us(slice_us);
        interval_us -= slice_us;
    }
}

void stepper_safe_init(void) {
    const unsigned int retired_pins[] = {13u, 14u};
    for (size_t i = 0; i < 2u; i++) {
        gpio_init(retired_pins[i]);
        gpio_put(retired_pins[i], 0);
        gpio_set_dir(retired_pins[i], GPIO_OUT);
    }
    for (size_t i = 0; i < sizeof(driver_pins) / sizeof(driver_pins[0]); i++) {
        gpio_init(driver_pins[i]);
        gpio_disable_pulls(driver_pins[i]);
        gpio_set_drive_strength(driver_pins[i], GPIO_DRIVE_STRENGTH_12MA);
        gpio_put(driver_pins[i], 0);
        gpio_set_dir(driver_pins[i], GPIO_OUT);
    }
    armed = false;
    all_off();
}

void stepper_init(void) {
    stepper_safe_init();
    const float divider = (float)clock_get_hz(clk_sys) / (float)PWM_COUNTER_HZ;
    bool configured_slices[NUM_PWM_SLICES] = {false};
    for (size_t i = 0; i < sizeof(driver_pins) / sizeof(driver_pins[0]); i++) {
        const unsigned int slice = pwm_gpio_to_slice_num(driver_pins[i]);
        pwm_set_gpio_level(driver_pins[i], 0u);
        if (!configured_slices[slice]) {
            pwm_config config = pwm_get_default_config();
            pwm_config_set_clkdiv(&config, divider);
            pwm_config_set_wrap(&config, PWM_WRAP);
            pwm_init(slice, &config, true);
            configured_slices[slice] = true;
        }
    }
    all_off();
}

void stepper_enable(bool enabled) {
    armed = enabled;
    if (!enabled) {
        all_off();
    }
}

bool stepper_enable_axis(char axis_name, bool enabled) {
    if (axis_name != 'X' && axis_name != 'Y') {
        return false;
    }
    stepper_enable(enabled);
    return true;
}

bool stepper_is_enabled(void) {
    return armed;
}

void stepper_set_position(float x_mm, float y_mm) {
    const float steps_per_mm = effective_steps_per_mm();
    commanded_position[AXIS_X] = x_mm;
    commanded_position[AXIS_Y] = y_mm;
    coordinate_origin[AXIS_X] = x_mm - (float)physical_step[AXIS_X] / steps_per_mm;
    coordinate_origin[AXIS_Y] = y_mm - (float)physical_step[AXIS_Y] / steps_per_mm;
}

void stepper_get_position(float *x_mm, float *y_mm) {
    if (x_mm) {
        *x_mm = commanded_position[AXIS_X];
    }
    if (y_mm) {
        *y_mm = commanded_position[AXIS_Y];
    }
}

bool stepper_diagnostic_step(char axis_name, bool positive) {
    (void)axis_name;
    (void)positive;
    return false;
}

bool stepper_diagnostic_pulses(char axis_name, bool positive,
                               unsigned int count, unsigned int interval_us) {
    (void)axis_name;
    (void)positive;
    (void)count;
    (void)interval_us;
    return false;
}

bool stepper_line_to(float x_mm, float y_mm, float feed_mm_min,
                     volatile bool *paused, volatile bool *cancelled,
                     stepper_realtime_callback_t realtime_callback,
                     void *realtime_context) {
    if (!armed) {
        return false;
    }

    const float steps_per_mm = effective_steps_per_mm();
    const long target[AXIS_COUNT] = {
        drv8835_target_step(x_mm, coordinate_origin[AXIS_X], steps_per_mm),
        drv8835_target_step(y_mm, coordinate_origin[AXIS_Y], steps_per_mm),
    };
    const long delta_x = target[AXIS_X] - physical_step[AXIS_X];
    const long delta_y = target[AXIS_Y] - physical_step[AXIS_Y];
    const long count_x = labs(delta_x);
    const long count_y = labs(delta_y);
    const long total = count_x > count_y ? count_x : count_y;

    if (total == 0) {
        commanded_position[AXIS_X] = x_mm;
        commanded_position[AXIS_Y] = y_mm;
        return true;
    }

    const bool positive_x = delta_x >= 0;
    const bool positive_y = delta_y >= 0;
    const float dx_mm = x_mm - commanded_position[AXIS_X];
    const float dy_mm = y_mm - commanded_position[AXIS_Y];
    const float distance = sqrtf(dx_mm * dx_mm + dy_mm * dy_mm);
    const float feed = feed_mm_min > 0.0f ? feed_mm_min : DEFAULT_FEED_MM_MIN;
    const float seconds = distance > 0.0f ? distance * 60.0f / feed : 0.0f;
    unsigned int interval_us = seconds > 0.0f
        ? (unsigned int)(seconds * 1000000.0f / (float)total)
        : 1000u;
    if (interval_us < 50u) {
        interval_us = 50u;
    }

    long error_x = 0;
    long error_y = 0;
    bool completed = true;
    const unsigned int next_active_axis_mask =
        drv8835_active_axis_mask(delta_x, delta_y, active_axes_only);
    const unsigned int previous_active_axis_mask =
        outputs_active ? active_axis_mask : 0u;
    const unsigned int newly_energized_mask = drv8835_newly_energized_axes(
        previous_active_axis_mask, next_active_axis_mask, outputs_active);
    const bool capture_required = newly_energized_mask != 0u;
    active_axis_mask = next_active_axis_mask;
    /* Continued axes, including reversals, do not restart boost in phase 1. */
    prepare_motion_boost(next_active_axis_mask, newly_energized_mask);
    apply_outputs();
    if (capture_required && capture_ms > 0u) {
        wait_motion_interval(capture_ms * 1000u, cancelled,
                             realtime_callback, realtime_context);
        if (cancelled && *cancelled) {
            all_off();
            return false;
        }
    }
    for (long i = 0; i < total; i++) {
        if (realtime_callback) {
            realtime_callback(realtime_context);
        }
        if (cancelled && *cancelled) {
            completed = false;
            break;
        }
        while (paused && *paused) {
            if (realtime_callback) {
                realtime_callback(realtime_context);
            }
            if (cancelled && *cancelled) {
                completed = false;
                break;
            }
            sleep_ms(5);
        }
        if (!completed) {
            break;
        }

        unsigned int completed_boost_mask = 0u;
        if (drv8835_bresenham_step(&error_x, count_x, total)) {
            if (prepare_axis_step(AXIS_X)) {
                completed_boost_mask |= 1u << AXIS_X;
            }
            physical_step[AXIS_X] += positive_x ? 1 : -1;
            angle[AXIS_X] = drv8835_wrap_angle(
                angle[AXIS_X] + drv8835_angle_step(microsteps, positive_x));
        }
        if (drv8835_bresenham_step(&error_y, count_y, total)) {
            if (prepare_axis_step(AXIS_Y)) {
                completed_boost_mask |= 1u << AXIS_Y;
            }
            physical_step[AXIS_Y] += positive_y ? 1 : -1;
            angle[AXIS_Y] = drv8835_wrap_angle(
                angle[AXIS_Y] + drv8835_angle_step(microsteps, positive_y));
        }
        apply_outputs();
        for (unsigned int axis = 0u; axis < AXIS_COUNT; axis++) {
            const unsigned int axis_bit = 1u << axis;
            if ((completed_boost_mask & axis_bit) != 0u) {
                boost_active_mask &= ~axis_bit;
                applied_peak_duty[axis] = run_peak_duty[axis];
            }
        }
        wait_motion_interval(interval_us, cancelled, realtime_callback,
                             realtime_context);
        if (cancelled && *cancelled) {
            completed = false;
            break;
        }
    }

    if (completed) {
        commanded_position[AXIS_X] = x_mm;
        commanded_position[AXIS_Y] = y_mm;
        /* Holding always uses run duty; pending boost progress is retained. */
        applied_peak_duty[AXIS_X] = run_peak_duty[AXIS_X];
        applied_peak_duty[AXIS_Y] = run_peak_duty[AXIS_Y];
        apply_outputs();
        schedule_release();
    } else {
        commanded_position[AXIS_X] = coordinate_origin[AXIS_X] +
            (float)physical_step[AXIS_X] / steps_per_mm;
        commanded_position[AXIS_Y] = coordinate_origin[AXIS_Y] +
            (float)physical_step[AXIS_Y] / steps_per_mm;
        all_off();
    }
    return completed;
}

void stepper_service(void) {
    if (outputs_active && hold_ms > 0u && time_reached(release_deadline)) {
        all_off();
    }
}

bool stepper_drv8835_configure(unsigned int new_microsteps,
                               unsigned int x_run_peak_duty,
                               unsigned int y_run_peak_duty,
                               unsigned int x_start_duty,
                               unsigned int y_start_duty,
                               unsigned int new_boost_steps,
                               unsigned int new_boost_ramp_steps,
                               unsigned int new_hold_ms,
                               unsigned int new_capture_ms,
                               bool new_active_axes_only) {
    if (armed || outputs_active || !drv8835_valid_microsteps(new_microsteps) ||
        x_run_peak_duty < 1u || x_run_peak_duty > 100u ||
        y_run_peak_duty < 1u || y_run_peak_duty > 100u ||
        x_start_duty < 1u || x_start_duty > 100u ||
        y_start_duty < 1u || y_start_duty > 100u ||
        new_boost_steps > DRV8835_MAX_BOOST_STEPS ||
        new_boost_ramp_steps > DRV8835_MAX_BOOST_STEPS ||
        new_hold_ms > 5000u || new_capture_ms > 500u) {
        return false;
    }
    microsteps = new_microsteps;
    run_peak_duty[AXIS_X] = x_run_peak_duty;
    run_peak_duty[AXIS_Y] = y_run_peak_duty;
    start_peak_duty[AXIS_X] = x_start_duty;
    start_peak_duty[AXIS_Y] = y_start_duty;
    applied_peak_duty[AXIS_X] = x_run_peak_duty;
    applied_peak_duty[AXIS_Y] = y_run_peak_duty;
    boost_steps = new_boost_steps;
    boost_ramp_steps = new_boost_ramp_steps;
    hold_ms = new_hold_ms;
    capture_ms = new_capture_ms;
    active_axes_only = new_active_axes_only;
    angle[AXIS_X] = 0;
    angle[AXIS_Y] = 0;
    physical_step[AXIS_X] = 0;
    physical_step[AXIS_Y] = 0;
    coordinate_origin[AXIS_X] = commanded_position[AXIS_X];
    coordinate_origin[AXIS_Y] = commanded_position[AXIS_Y];
    return true;
}

void stepper_drv8835_get_config(unsigned int *configured_microsteps,
                                unsigned int *x_run_duty,
                                unsigned int *y_run_duty,
                                unsigned int *x_start_duty,
                                unsigned int *y_start_duty,
                                unsigned int *configured_boost_steps,
                                unsigned int *configured_boost_ramp_steps,
                                unsigned int *configured_hold_ms,
                                unsigned int *configured_capture_ms,
                                bool *configured_active_axes_only) {
    if (configured_microsteps) {
        *configured_microsteps = microsteps;
    }
    if (x_run_duty) {
        *x_run_duty = run_peak_duty[AXIS_X];
    }
    if (y_run_duty) {
        *y_run_duty = run_peak_duty[AXIS_Y];
    }
    if (x_start_duty) {
        *x_start_duty = start_peak_duty[AXIS_X];
    }
    if (y_start_duty) {
        *y_start_duty = start_peak_duty[AXIS_Y];
    }
    if (configured_boost_steps) {
        *configured_boost_steps = boost_steps;
    }
    if (configured_boost_ramp_steps) {
        *configured_boost_ramp_steps = boost_ramp_steps;
    }
    if (configured_hold_ms) {
        *configured_hold_ms = hold_ms;
    }
    if (configured_capture_ms) {
        *configured_capture_ms = capture_ms;
    }
    if (configured_active_axes_only) {
        *configured_active_axes_only = active_axes_only;
    }
}

void stepper_drv8835_get_motion_status(unsigned int *configured_active_mask,
                                       unsigned int *configured_boost_active_mask,
                                       unsigned int *x_applied_duty,
                                       unsigned int *y_applied_duty,
                                       int *x_angle,
                                       int *y_angle,
                                       const char **x_phase,
                                       const char **y_phase) {
    if (configured_active_mask) {
        *configured_active_mask = active_axis_mask;
    }
    if (configured_boost_active_mask) {
        *configured_boost_active_mask = boost_active_mask;
    }
    if (x_applied_duty) {
        *x_applied_duty = applied_peak_duty[AXIS_X];
    }
    if (y_applied_duty) {
        *y_applied_duty = applied_peak_duty[AXIS_Y];
    }
    if (x_angle) {
        *x_angle = angle[AXIS_X];
    }
    if (y_angle) {
        *y_angle = angle[AXIS_Y];
    }
    if (x_phase) {
        *x_phase = phase_name(AXIS_X, angle[AXIS_X]);
    }
    if (y_phase) {
        *y_phase = phase_name(AXIS_Y, angle[AXIS_Y]);
    }
}

bool stepper_drv8835_outputs_active(void) {
    return outputs_active;
}

void stepper_drv8835_report_status(void) {
    const bool boost_enabled =
        drv8835_boost_enabled(run_peak_duty[AXIS_X], start_peak_duty[AXIS_X],
                              boost_steps, boost_ramp_steps) ||
        drv8835_boost_enabled(run_peak_duty[AXIS_Y], start_peak_duty[AXIS_Y],
                              boost_steps, boost_ramp_steps);
    const unsigned int boost_output_mask =
        (outputs_active && applied_peak_duty[AXIS_X] != run_peak_duty[AXIS_X]
             ? 1u << AXIS_X : 0u) |
        (outputs_active && applied_peak_duty[AXIS_Y] != run_peak_duty[AXIS_Y]
             ? 1u << AXIS_Y : 0u);
    printf("[MSG:DRV8835 armed=%u outputs=%s microsteps=%u step_mm=%.5f "
           "drive_mode=%s X_peak=%u Y_peak=%u X_run=%u Y_run=%u "
           "X_start=%u Y_start=%u "
           "boost_steps=%u boost_ramp_steps=%u boost_enabled=%u "
           "boost_active_mask=%u boost_pending_mask=%u X_state=%s "
           "Y_state=%s X_applied=%u "
           "Y_applied=%u hold_ms=%u capture_ms=%u "
           "axis_mode=%s active_mask=%u X_angle=%d X_phase=%s X_coils=%d/%d "
           "Y_angle=%d Y_phase=%s Y_coils=%d/%d physical_steps=%ld/%ld "
           "position=%.3f/%.3f]\r\n",
           armed ? 1u : 0u, outputs_active ? "ON" : "HiZ", microsteps,
           (double)(1.0f / effective_steps_per_mm()),
           microsteps == 1u ? "single-phase" : "microstep",
           run_peak_duty[AXIS_X], run_peak_duty[AXIS_Y],
           run_peak_duty[AXIS_X], run_peak_duty[AXIS_Y],
           start_peak_duty[AXIS_X], start_peak_duty[AXIS_Y], boost_steps,
           boost_ramp_steps, boost_enabled ? 1u : 0u, boost_output_mask,
           boost_active_mask,
           (boost_output_mask & (1u << AXIS_X)) != 0u ? "boost" : "run",
           (boost_output_mask & (1u << AXIS_Y)) != 0u ? "boost" : "run",
           applied_peak_duty[AXIS_X], applied_peak_duty[AXIS_Y], hold_ms,
           capture_ms,
           active_axes_only ? "moving-only" : "both", active_axis_mask,
           angle[AXIS_X], phase_name(AXIS_X, angle[AXIS_X]),
           coil_duty[AXIS_X][0], coil_duty[AXIS_X][1], angle[AXIS_Y],
           phase_name(AXIS_Y, angle[AXIS_Y]),
           coil_duty[AXIS_Y][0], coil_duty[AXIS_Y][1],
           physical_step[AXIS_X], physical_step[AXIS_Y],
           (double)commanded_position[AXIS_X],
           (double)commanded_position[AXIS_Y]);
}
