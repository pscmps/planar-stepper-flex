#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "position_sensor_math.h"

static void expect_near(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.001f);
}

int main(void) {
    expect_near(as5600_raw_degrees(0u), 0.0f);
    expect_near(as5600_raw_degrees(1024u), 90.0f);
    expect_near(as5600_raw_degrees(2048u), 180.0f);
    expect_near(as5600_raw_degrees(3072u), 270.0f);
    expect_near(as5600_raw_degrees(4095u), 359.912109375f);
    expect_near(as5600_raw_degrees(0x1c00u), 270.0f);
    puts("position sensor math tests passed");
    return 0;
}
