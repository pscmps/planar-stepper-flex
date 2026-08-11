#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool present;
    bool magnet_detected;
    uint16_t raw;
    uint8_t status;
    float degrees;
    uint32_t updated_ms;
} position_sensor_snapshot_t;

void position_sensors_init(void);
void position_sensors_refresh(void);
void position_sensors_service(void);
void position_sensors_set_status_enabled(bool enabled);
bool position_sensors_status_enabled(void);
void position_sensors_get_snapshot(unsigned int joint,
                                   position_sensor_snapshot_t *snapshot);
void position_sensors_report(void);
