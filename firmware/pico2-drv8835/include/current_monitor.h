#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool present;
    uint16_t raw;
    uint16_t zero;
    float average_a;
    float absolute_average_a;
    float minimum_a;
    float maximum_a;
} current_monitor_snapshot_t;

void current_monitor_init(void);
void current_monitor_service(void);
bool current_monitor_calibrate(void);
void current_monitor_get_snapshot(current_monitor_snapshot_t *snapshot);
void current_monitor_report(void);
