#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "drv8835_motion_math.h"

static void test_supported_microsteps(void) {
    assert(drv8835_valid_microsteps(1));
    assert(drv8835_valid_microsteps(2));
    assert(drv8835_valid_microsteps(4));
    assert(drv8835_valid_microsteps(8));
    assert(drv8835_valid_microsteps(16));
    assert(!drv8835_valid_microsteps(3));
    assert(!drv8835_valid_microsteps(32));
    assert(drv8835_angle_step(8, true) == 8);
    assert(drv8835_angle_step(8, false) == -8);
}

static void test_phase_order_and_duty(void) {
    int coil_0 = 0;
    int coil_1 = 0;
    drv8835_axis_duties(0, 100, &coil_0, &coil_1);
    assert(coil_0 == 100 && coil_1 == 0);
    drv8835_axis_duties(64, 100, &coil_0, &coil_1);
    assert(coil_0 == 0 && coil_1 == -100);
    drv8835_axis_duties(128, 100, &coil_0, &coil_1);
    assert(coil_0 == -100 && coil_1 == 0);
    drv8835_axis_duties(192, 100, &coil_0, &coil_1);
    assert(coil_0 == 0 && coil_1 == 100);
    drv8835_axis_duties(32, 100, &coil_0, &coil_1);
    assert(coil_0 == 71 && coil_1 == -71);
    drv8835_axis_duties(0, 150, &coil_0, &coil_1);
    assert(coil_0 == 100 && coil_1 == 0);
}

static void test_start_boost_transition(void) {
    assert(drv8835_boost_enabled(60u, 100u, 2u, 4u));
    assert(drv8835_boost_duty_for_step(60u, 100u, 2u, 4u, 0u) == 100u);
    assert(drv8835_boost_duty_for_step(60u, 100u, 2u, 4u, 1u) == 100u);
    assert(drv8835_boost_duty_for_step(60u, 100u, 2u, 4u, 2u) == 90u);
    assert(drv8835_boost_duty_for_step(60u, 100u, 2u, 4u, 3u) == 80u);
    assert(drv8835_boost_duty_for_step(60u, 100u, 2u, 4u, 4u) == 70u);
    assert(drv8835_boost_duty_for_step(60u, 100u, 2u, 4u, 5u) == 60u);
    assert(drv8835_boost_duty_for_step(60u, 100u, 2u, 4u, 6u) == 60u);

    assert(drv8835_boost_duty_for_step(80u, 40u, 1u, 4u, 0u) == 40u);
    assert(drv8835_boost_duty_for_step(80u, 40u, 1u, 4u, 1u) == 50u);
    assert(drv8835_boost_duty_for_step(80u, 40u, 1u, 4u, 4u) == 80u);
}

static void test_boost_disabled_and_clamped(void) {
    assert(!drv8835_boost_enabled(60u, 100u, 0u, 0u));
    assert(drv8835_boost_duty_for_step(60u, 100u, 0u, 0u, 0u) == 60u);
    assert(!drv8835_boost_enabled(100u, 150u, 8u, 16u));
    assert(drv8835_boost_duty_for_step(40u, 150u, 1u, 0u, 0u) == 100u);
    assert(drv8835_boost_duty_for_step(40u, 150u, 1u, 0u, 1u) == 40u);
    assert(drv8835_clamp_duty(101u) == 100u);
}

static void test_boost_is_direction_independent(void) {
    for (unsigned int step = 0; step < 8u; step++) {
        const unsigned int peak =
            drv8835_boost_duty_for_step(60u, 100u, 2u, 4u, step);
        int positive_0 = 0;
        int positive_1 = 0;
        int negative_0 = 0;
        int negative_1 = 0;
        drv8835_axis_duties(32, peak, &positive_0, &positive_1);
        drv8835_axis_duties(-32, peak, &negative_0, &negative_1);
        assert(abs(positive_0) == abs(negative_0));
        assert(abs(positive_1) == abs(negative_1));
    }
}

static void test_u1_is_single_phase(void) {
    int coil_0 = 0;
    int coil_1 = 0;
    int angle = 0;
    const int expected[4][2] = {
        {100, 0}, {0, -100}, {-100, 0}, {0, 100},
    };
    for (unsigned int step = 0; step < 4u; step++) {
        drv8835_axis_duties(angle, 100, &coil_0, &coil_1);
        assert(coil_0 == expected[step][0]);
        assert(coil_1 == expected[step][1]);
        assert((coil_0 == 0) != (coil_1 == 0));
        angle = drv8835_wrap_angle(angle + drv8835_angle_step(1, true));
    }
    assert(angle == 0);
}

static void test_absolute_coordinate_quantization(void) {
    const float steps_per_mm = 3.2f;
    assert(drv8835_target_step(0.1f, 0.0f, steps_per_mm) == 0);
    assert(drv8835_target_step(0.2f, 0.0f, steps_per_mm) == 1);
    assert(drv8835_target_step(1.0f, 0.0f, steps_per_mm) == 3);
    assert(drv8835_target_step(2.0f, 0.0f, steps_per_mm) == 6);
    assert(drv8835_target_step(10.0f, 0.0f, steps_per_mm) == 32);
    assert(drv8835_target_step(-1.0f, 0.0f, steps_per_mm) == -3);
    assert(drv8835_target_step(5.0f, 5.0f, steps_per_mm) == 0);
}

static void test_xy_bresenham_interpolation(void) {
    long x_error = 0;
    long y_error = 0;
    unsigned int x_events = 0;
    unsigned int y_events = 0;
    const bool expected_y[] = {false, true, false, true};
    for (unsigned int i = 0; i < 4u; i++) {
        if (drv8835_bresenham_step(&x_error, 4, 4)) {
            x_events++;
        }
        const bool y_due = drv8835_bresenham_step(&y_error, 2, 4);
        assert(y_due == expected_y[i]);
        if (y_due) {
            y_events++;
        }
    }
    assert(x_events == 4u);
    assert(y_events == 2u);
    assert(x_error == 0);
    assert(y_error == 0);
}

static void test_active_axis_mask(void) {
    assert(drv8835_active_axis_mask(4, 0, true) == 0x1u);
    assert(drv8835_active_axis_mask(0, -4, true) == 0x2u);
    assert(drv8835_active_axis_mask(4, -4, true) == 0x3u);
    assert(drv8835_active_axis_mask(4, 0, false) == 0x3u);
}

static void test_capture_decision(void) {
    assert(drv8835_capture_required(0u, 0x1u, false));
    assert(drv8835_capture_required(0x1u, 0x2u, true));
    assert(drv8835_capture_required(0x1u, 0x3u, true));
    assert(!drv8835_capture_required(0x1u, 0x1u, true));
    assert(!drv8835_capture_required(0x3u, 0x1u, true));
    assert(drv8835_newly_energized_axes(0u, 0x1u, false) == 0x1u);
    assert(drv8835_newly_energized_axes(0x1u, 0x1u, true) == 0u);
    assert(drv8835_newly_energized_axes(0x1u, 0x3u, true) == 0x2u);
    assert(drv8835_newly_energized_axes(0x3u, 0x1u, true) == 0u);
    assert(drv8835_newly_energized_axes(0x1u, 0x1u, false) == 0x1u);
}

int main(void) {
    test_supported_microsteps();
    test_phase_order_and_duty();
    test_start_boost_transition();
    test_boost_disabled_and_clamped();
    test_boost_is_direction_independent();
    test_u1_is_single_phase();
    test_absolute_coordinate_quantization();
    test_xy_bresenham_interpolation();
    test_active_axis_mask();
    test_capture_decision();
    puts("drv8835 motion math tests passed");
    return 0;
}
