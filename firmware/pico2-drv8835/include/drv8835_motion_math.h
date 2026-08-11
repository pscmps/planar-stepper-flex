#pragma once

#include <stdbool.h>

#define DRV8835_ELECTRICAL_CYCLE_UNITS 256
#define DRV8835_QUADRANT_UNITS 64
#define DRV8835_MAX_BOOST_STEPS 10000u

bool drv8835_valid_microsteps(unsigned int microsteps);
int drv8835_wrap_angle(int angle);
int drv8835_angle_step(unsigned int microsteps, bool positive);
void drv8835_axis_duties(int angle, unsigned int peak_duty,
                         int *coil_0_duty, int *coil_1_duty);
unsigned int drv8835_clamp_duty(unsigned int duty);
bool drv8835_boost_enabled(unsigned int run_peak_duty,
                           unsigned int start_peak_duty,
                           unsigned int boost_steps,
                           unsigned int boost_ramp_steps);
unsigned int drv8835_boost_duty_for_step(unsigned int run_peak_duty,
                                          unsigned int start_peak_duty,
                                          unsigned int boost_steps,
                                          unsigned int boost_ramp_steps,
                                          unsigned int axis_step_index);
long drv8835_target_step(float position_mm, float origin_mm,
                         float steps_per_mm);
bool drv8835_bresenham_step(long *accumulator, long axis_steps,
                            long total_steps);
unsigned int drv8835_active_axis_mask(long delta_x, long delta_y,
                                      bool active_axes_only);
unsigned int drv8835_newly_energized_axes(unsigned int previous_mask,
                                           unsigned int next_mask,
                                           bool outputs_active);
bool drv8835_capture_required(unsigned int previous_mask,
                              unsigned int next_mask,
                              bool outputs_active);
