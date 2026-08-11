#pragma once

#include <stdbool.h>

#include "config.h"

typedef struct {
    unsigned int step_pin;
    unsigned int dir_pin;
    unsigned int enable_pin;
    bool dir_invert;
    bool enable_active_low;
    float steps_per_mm;
} stepper_axis_t;

typedef void (*stepper_realtime_callback_t)(void *context);

void stepper_init(void);
void stepper_safe_init(void);
void stepper_enable(bool enabled);
bool stepper_enable_axis(char axis, bool enabled);
bool stepper_is_enabled(void);
void stepper_set_position(float x_mm, float y_mm);
void stepper_get_position(float *x_mm, float *y_mm);
bool stepper_diagnostic_step(char axis, bool positive);
bool stepper_diagnostic_pulses(char axis, bool positive, unsigned int count, unsigned int interval_us);
bool stepper_line_to(float x_mm, float y_mm, float feed_mm_min,
                     volatile bool *paused, volatile bool *cancelled,
                     stepper_realtime_callback_t realtime_callback,
                     void *realtime_context);
void stepper_service(void);

bool stepper_drv8835_configure(unsigned int microsteps,
                               unsigned int x_run_peak_duty,
                               unsigned int y_run_peak_duty,
                               unsigned int x_start_peak_duty,
                               unsigned int y_start_peak_duty,
                               unsigned int boost_steps,
                               unsigned int boost_ramp_steps,
                               unsigned int hold_ms,
                               unsigned int capture_ms,
                               bool active_axes_only);
void stepper_drv8835_get_config(unsigned int *microsteps,
                                unsigned int *x_run_peak_duty,
                                unsigned int *y_run_peak_duty,
                                unsigned int *x_start_peak_duty,
                                unsigned int *y_start_peak_duty,
                                unsigned int *boost_steps,
                                unsigned int *boost_ramp_steps,
                                unsigned int *hold_ms,
                                unsigned int *capture_ms,
                                bool *active_axes_only);
void stepper_drv8835_get_motion_status(unsigned int *active_mask,
                                       unsigned int *boost_active_mask,
                                       unsigned int *x_applied_peak_duty,
                                       unsigned int *y_applied_peak_duty,
                                       int *x_angle,
                                       int *y_angle,
                                       const char **x_phase,
                                       const char **y_phase);
bool stepper_drv8835_outputs_active(void);
void stepper_drv8835_report_status(void);
