#pragma once

#include <stdint.h>

static inline float current_monitor_adc_volts(uint16_t raw) {
    return 3.3f * (float)raw / 4095.0f;
}

static inline float current_monitor_amps(uint16_t raw, uint16_t zero,
                                         float sensitivity_v_per_a) {
    if (sensitivity_v_per_a == 0.0f) {
        return 0.0f;
    }
    return (current_monitor_adc_volts(raw) -
            current_monitor_adc_volts(zero)) / sensitivity_v_per_a;
}
