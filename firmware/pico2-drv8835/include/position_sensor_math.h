#pragma once

#include <stdint.h>

static inline float as5600_raw_degrees(uint16_t raw) {
    return (float)(raw & 0x0fffu) * (360.0f / 4096.0f);
}
