#pragma once

#include <stdbool.h>

#define SERVO_PEN_MIN_PULSE_US 500u
#define SERVO_PEN_MAX_PULSE_US 2500u
#define SERVO_PEN_MAX_SETTLE_MS 2000u

typedef struct {
    unsigned int up_pulse_us;
    unsigned int down_pulse_us;
    unsigned int settle_ms;
    float z_threshold;
} servo_pen_config_t;

bool servo_pen_config_valid(const servo_pen_config_t *config);
bool servo_pen_z_is_up(float z, float threshold);
