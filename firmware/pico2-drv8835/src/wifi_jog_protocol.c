#include "wifi_jog_protocol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *parameter_value(int count, char *names[], char *values[],
                                   const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            return values[i];
        }
    }
    return NULL;
}

static bool allowed_distance(float distance) {
    static const float allowed[] = {0.3125f, 0.625f, 2.5f, 10.0f};
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
        if (fabsf(distance - allowed[i]) < 0.0001f) {
            return true;
        }
    }
    return false;
}

bool wifi_jog_parse_params(int count, char *names[], char *values[],
                           wifi_jog_request_t *request) {
    if (!request) {
        return false;
    }
    const char *axis = parameter_value(count, names, values, "axis");
    const char *direction = parameter_value(count, names, values, "direction");
    const char *distance = parameter_value(count, names, values, "distance");
    const char *feed = parameter_value(count, names, values, "feed");
    if (!axis || !direction || !distance || !feed || axis[1] != '\0' ||
        (axis[0] != 'X' && axis[0] != 'Y')) {
        return false;
    }

    char *end = NULL;
    const long parsed_direction = strtol(direction, &end, 10);
    if (*direction == '\0' || *end != '\0' ||
        (parsed_direction != -1 && parsed_direction != 1)) {
        return false;
    }
    end = NULL;
    const float parsed_distance = strtof(distance, &end);
    if (*distance == '\0' || *end != '\0' || !isfinite(parsed_distance) ||
        !allowed_distance(parsed_distance)) {
        return false;
    }
    end = NULL;
    const long parsed_feed = strtol(feed, &end, 10);
    if (*feed == '\0' || *end != '\0' || parsed_feed < 30 || parsed_feed > 1000) {
        return false;
    }

    request->axis = axis[0];
    request->direction = (int)parsed_direction;
    request->distance_mm = parsed_distance;
    request->feed_mm_min = (unsigned int)parsed_feed;
    return true;
}

bool wifi_mode_parse_params(int count, char *names[], char *values[],
                            wifi_drive_request_t *request) {
    if (!request) {
        return false;
    }
    const char *active_only = parameter_value(count, names, values, "activeOnly");
    const char *microsteps = parameter_value(count, names, values, "microsteps");
    const char *capture_ms = parameter_value(count, names, values, "captureMs");
    if (!active_only || !microsteps || !capture_ms ||
        (strcmp(active_only, "0") != 0 && strcmp(active_only, "1") != 0)) {
        return false;
    }
    char *end = NULL;
    const long parsed_microsteps = strtol(microsteps, &end, 10);
    if (*microsteps == '\0' || *end != '\0' ||
        (parsed_microsteps != 1 && parsed_microsteps != 8)) {
        return false;
    }
    end = NULL;
    const long parsed_capture_ms = strtol(capture_ms, &end, 10);
    if (*capture_ms == '\0' || *end != '\0' ||
        parsed_capture_ms < 0 || parsed_capture_ms > 500) {
        return false;
    }
    request->active_axes_only = active_only[0] == '1';
    request->microsteps = (unsigned int)parsed_microsteps;
    request->capture_ms = (unsigned int)parsed_capture_ms;
    return true;
}

bool wifi_pen_parse_params(int count, char *names[], char *values[],
                           bool *pen_up) {
    if (!pen_up) {
        return false;
    }
    const char *state = parameter_value(count, names, values, "state");
    if (!state) {
        return false;
    }
    if (strcmp(state, "up") == 0) {
        *pen_up = true;
        return true;
    }
    if (strcmp(state, "down") == 0) {
        *pen_up = false;
        return true;
    }
    return false;
}

bool wifi_jog_queue_offer(wifi_jog_request_t *slot, bool *pending,
                          const wifi_jog_request_t *request) {
    if (!slot || !pending || !request || *pending) {
        return false;
    }
    *slot = *request;
    *pending = true;
    return true;
}

bool wifi_jog_queue_take(wifi_jog_request_t *slot, bool *pending,
                         wifi_jog_request_t *request) {
    if (!slot || !pending || !request || !*pending) {
        return false;
    }
    *request = *slot;
    *pending = false;
    return true;
}

int wifi_jog_build_gcode(const wifi_jog_request_t *request,
                         char *buffer, size_t buffer_size) {
    if (!request || !buffer || buffer_size == 0u) {
        return -1;
    }
    return snprintf(buffer, buffer_size, "$J=G91 G21 %c%.4f F%u",
                    request->axis,
                    (double)(request->distance_mm * (float)request->direction),
                    request->feed_mm_min);
}

int wifi_jog_format_status(const wifi_jog_status_t *status,
                           char *buffer, size_t buffer_size) {
    if (!status || !buffer || buffer_size == 0u) {
        return -1;
    }
    const char *result = status->last_result ? status->last_result : "unknown";
    return snprintf(buffer, buffer_size,
                    "{\"ready\":%s,\"busy\":%s,\"armed\":%s,"
                    "\"outputs\":%s,\"activeOnly\":%s,\"microsteps\":%u,"
                    "\"captureMs\":%u,\"activeMask\":%u,"
                    "\"xAngle\":%d,\"yAngle\":%d,"
                    "\"xPhase\":\"%s\",\"yPhase\":\"%s\",\"clients\":%u,"
                    "\"x\":%.4f,\"y\":%.4f,\"z\":%.4f,"
                    "\"penPulseUs\":%u,\"penUp\":%s,"
                    "\"lastResult\":\"%s\"}",
                    status->ready ? "true" : "false",
                    status->busy ? "true" : "false",
                    status->armed ? "true" : "false",
                    status->outputs ? "true" : "false",
                    status->active_axes_only ? "true" : "false",
                    status->microsteps, status->capture_ms, status->active_mask,
                    status->x_angle, status->y_angle,
                    status->x_phase ? status->x_phase : "?",
                    status->y_phase ? status->y_phase : "?",
                    status->clients,
                    (double)status->x, (double)status->y, (double)status->z,
                    status->pen_pulse_us,
                    status->pen_up ? "true" : "false", result);
}

int wifi_jog_make_ssid(const char *unique_id, char *buffer, size_t buffer_size) {
    if (!unique_id || !buffer || buffer_size == 0u) {
        return -1;
    }
    const size_t length = strlen(unique_id);
    const char *suffix = length > 4u ? unique_id + length - 4u : unique_id;
    return snprintf(buffer, buffer_size, "PlanarStepper-%s", suffix);
}
