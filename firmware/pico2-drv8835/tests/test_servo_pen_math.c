#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "servo_pen_math.h"

static void test_config_ranges(void) {
    servo_pen_config_t config = {
        .up_pulse_us = 1400u,
        .down_pulse_us = 1000u,
        .settle_ms = 150u,
        .z_threshold = 0.5f,
    };
    assert(servo_pen_config_valid(&config));
    config.up_pulse_us = 499u;
    assert(!servo_pen_config_valid(&config));
    config.up_pulse_us = 1400u;
    config.down_pulse_us = 2501u;
    assert(!servo_pen_config_valid(&config));
    config.down_pulse_us = 1000u;
    config.settle_ms = SERVO_PEN_MAX_SETTLE_MS + 1u;
    assert(!servo_pen_config_valid(&config));
    config.settle_ms = 0u;
    config.z_threshold = NAN;
    assert(!servo_pen_config_valid(&config));
}

static void test_z_threshold(void) {
    assert(!servo_pen_z_is_up(0.0f, 0.5f));
    assert(!servo_pen_z_is_up(0.5f, 0.5f));
    assert(servo_pen_z_is_up(0.5001f, 0.5f));
    assert(servo_pen_z_is_up(5.0f, 0.5f));
    assert(!servo_pen_z_is_up(-1.0f, 0.5f));
}

int main(void) {
    test_config_ranges();
    test_z_threshold();
    puts("servo_pen_math tests passed");
    return 0;
}
