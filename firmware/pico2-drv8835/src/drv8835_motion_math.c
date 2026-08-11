#include "drv8835_motion_math.h"

#include <math.h>

#define TWO_PI_F 6.28318530717958647692f

bool drv8835_valid_microsteps(unsigned int microsteps) {
    return microsteps == 1u || microsteps == 2u || microsteps == 4u ||
           microsteps == 8u || microsteps == 16u;
}

int drv8835_wrap_angle(int angle) {
    angle %= DRV8835_ELECTRICAL_CYCLE_UNITS;
    return angle < 0 ? angle + DRV8835_ELECTRICAL_CYCLE_UNITS : angle;
}

int drv8835_angle_step(unsigned int microsteps, bool positive) {
    if (!drv8835_valid_microsteps(microsteps)) {
        return 0;
    }
    const int step = DRV8835_QUADRANT_UNITS / (int)microsteps;
    return positive ? step : -step;
}

void drv8835_axis_duties(int angle, unsigned int peak_duty,
                         int *coil_0_duty, int *coil_1_duty) {
    peak_duty = drv8835_clamp_duty(peak_duty);
    const float radians = TWO_PI_F * (float)drv8835_wrap_angle(angle) /
                          (float)DRV8835_ELECTRICAL_CYCLE_UNITS;
    if (coil_0_duty) {
        *coil_0_duty = (int)lroundf(cosf(radians) * (float)peak_duty);
    }
    if (coil_1_duty) {
        *coil_1_duty = (int)lroundf(-sinf(radians) * (float)peak_duty);
    }
}

unsigned int drv8835_clamp_duty(unsigned int duty) {
    return duty > 100u ? 100u : duty;
}

bool drv8835_boost_enabled(unsigned int run_peak_duty,
                           unsigned int start_peak_duty,
                           unsigned int boost_steps,
                           unsigned int boost_ramp_steps) {
    return (boost_steps > 0u || boost_ramp_steps > 0u) &&
           drv8835_clamp_duty(run_peak_duty) !=
               drv8835_clamp_duty(start_peak_duty);
}

unsigned int drv8835_boost_duty_for_step(unsigned int run_peak_duty,
                                          unsigned int start_peak_duty,
                                          unsigned int boost_steps,
                                          unsigned int boost_ramp_steps,
                                          unsigned int axis_step_index) {
    const unsigned int run = drv8835_clamp_duty(run_peak_duty);
    const unsigned int start = drv8835_clamp_duty(start_peak_duty);
    if (!drv8835_boost_enabled(run, start, boost_steps, boost_ramp_steps)) {
        return run;
    }
    if (axis_step_index < boost_steps) {
        return start;
    }
    const unsigned int ramp_index = axis_step_index - boost_steps;
    if (boost_ramp_steps == 0u || ramp_index >= boost_ramp_steps) {
        return run;
    }

    const unsigned int ramp_position = ramp_index + 1u;
    const int delta = (int)run - (int)start;
    const int magnitude = delta >= 0 ? delta : -delta;
    const int rounded_change = (int)(
        ((unsigned int)magnitude * ramp_position + boost_ramp_steps / 2u) /
        boost_ramp_steps);
    const int duty = (int)start + (delta >= 0 ? rounded_change : -rounded_change);
    return drv8835_clamp_duty(duty > 0 ? (unsigned int)duty : 0u);
}

long drv8835_target_step(float position_mm, float origin_mm,
                         float steps_per_mm) {
    return lroundf((position_mm - origin_mm) * steps_per_mm);
}

bool drv8835_bresenham_step(long *accumulator, long axis_steps,
                            long total_steps) {
    if (!accumulator || axis_steps <= 0 || total_steps <= 0) {
        return false;
    }
    *accumulator += axis_steps;
    if (*accumulator < total_steps) {
        return false;
    }
    *accumulator -= total_steps;
    return true;
}

unsigned int drv8835_active_axis_mask(long delta_x, long delta_y,
                                      bool active_axes_only) {
    if (!active_axes_only) {
        return 0x3u;
    }
    return (delta_x != 0 ? 0x1u : 0u) | (delta_y != 0 ? 0x2u : 0u);
}

unsigned int drv8835_newly_energized_axes(unsigned int previous_mask,
                                           unsigned int next_mask,
                                           bool outputs_active) {
    const unsigned int energized_mask = outputs_active ? previous_mask : 0u;
    return next_mask & ~energized_mask;
}

bool drv8835_capture_required(unsigned int previous_mask,
                              unsigned int next_mask,
                              bool outputs_active) {
    return drv8835_newly_energized_axes(previous_mask, next_mask,
                                        outputs_active) != 0u;
}
