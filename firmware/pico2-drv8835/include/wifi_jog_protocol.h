#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char axis;
    int direction;
    float distance_mm;
    unsigned int feed_mm_min;
} wifi_jog_request_t;

typedef struct {
    unsigned int microsteps;
    unsigned int capture_ms;
    bool active_axes_only;
} wifi_drive_request_t;

typedef struct {
    bool ready;
    bool busy;
    bool armed;
    bool outputs;
    bool active_axes_only;
    unsigned int microsteps;
    unsigned int capture_ms;
    unsigned int active_mask;
    int x_angle;
    int y_angle;
    const char *x_phase;
    const char *y_phase;
    unsigned int clients;
    float x;
    float y;
    float z;
    unsigned int pen_pulse_us;
    bool pen_up;
    const char *last_result;
} wifi_jog_status_t;

bool wifi_jog_parse_params(int count, char *names[], char *values[],
                           wifi_jog_request_t *request);
bool wifi_mode_parse_params(int count, char *names[], char *values[],
                            wifi_drive_request_t *request);
bool wifi_pen_parse_params(int count, char *names[], char *values[],
                           bool *pen_up);
bool wifi_jog_queue_offer(wifi_jog_request_t *slot, bool *pending,
                          const wifi_jog_request_t *request);
bool wifi_jog_queue_take(wifi_jog_request_t *slot, bool *pending,
                         wifi_jog_request_t *request);
int wifi_jog_build_gcode(const wifi_jog_request_t *request,
                         char *buffer, size_t buffer_size);
int wifi_jog_format_status(const wifi_jog_status_t *status,
                           char *buffer, size_t buffer_size);
int wifi_jog_make_ssid(const char *unique_id, char *buffer, size_t buffer_size);
