#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "current_monitor_math.h"

static bool near(float actual, float expected, float tolerance) {
    return fabsf(actual - expected) <= tolerance;
}

int main(void) {
    assert(near(current_monitor_adc_volts(0u), 0.0f, 0.0001f));
    assert(near(current_monitor_adc_volts(4095u), 3.3f, 0.0001f));

    const uint16_t zero = 2048u;
    const uint16_t one_amp = (uint16_t)(
        zero + lroundf(0.132f * 4095.0f / 3.3f));
    assert(near(current_monitor_amps(one_amp, zero, 0.132f),
                1.0f, 0.02f));
    assert(near(current_monitor_amps(zero, zero, 0.132f),
                0.0f, 0.0001f));
    assert(near(current_monitor_amps(one_amp, zero, 0.0f),
                0.0f, 0.0001f));
    return 0;
}
