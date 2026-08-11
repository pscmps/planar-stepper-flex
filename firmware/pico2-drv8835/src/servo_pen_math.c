#include "servo_pen_math.h"

#include <math.h>

bool servo_pen_config_valid(const servo_pen_config_t *config) {
    return config &&
           config->up_pulse_us >= SERVO_PEN_MIN_PULSE_US &&
           config->up_pulse_us <= SERVO_PEN_MAX_PULSE_US &&
           config->down_pulse_us >= SERVO_PEN_MIN_PULSE_US &&
           config->down_pulse_us <= SERVO_PEN_MAX_PULSE_US &&
           config->settle_ms <= SERVO_PEN_MAX_SETTLE_MS &&
           isfinite(config->z_threshold);
}

bool servo_pen_z_is_up(float z, float threshold) {
    return isfinite(z) && isfinite(threshold) && z > threshold;
}
