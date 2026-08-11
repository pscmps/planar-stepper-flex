#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "wifi_jog_protocol.h"

static void test_parameters_and_gcode(void) {
    char *names[] = {"axis", "direction", "distance", "feed"};
    char *values[] = {"Y", "-1", "0.625", "300"};
    wifi_jog_request_t request;
    assert(wifi_jog_parse_params(4, names, values, &request));
    assert(request.axis == 'Y');
    assert(request.direction == -1);
    assert(request.feed_mm_min == 300u);

    char command[64];
    assert(wifi_jog_build_gcode(&request, command, sizeof(command)) > 0);
    assert(strcmp(command, "$J=G91 G21 Y-0.6250 F300") == 0);

    values[2] = "1.0";
    assert(!wifi_jog_parse_params(4, names, values, &request));
    values[2] = "2.5";
    values[3] = "1001";
    assert(!wifi_jog_parse_params(4, names, values, &request));

    char *mode_names[] = {"activeOnly", "microsteps", "captureMs"};
    char *mode_values[] = {"1", "1", "100"};
    wifi_drive_request_t drive;
    assert(wifi_mode_parse_params(3, mode_names, mode_values, &drive));
    assert(drive.active_axes_only && drive.microsteps == 1u &&
           drive.capture_ms == 100u);
    mode_values[1] = "8";
    assert(wifi_mode_parse_params(3, mode_names, mode_values, &drive));
    assert(drive.microsteps == 8u);
    mode_values[0] = "2";
    assert(!wifi_mode_parse_params(3, mode_names, mode_values, &drive));

    char *pen_names[] = {"state"};
    char *pen_values[] = {"up"};
    bool pen_up = false;
    assert(wifi_pen_parse_params(1, pen_names, pen_values, &pen_up));
    assert(pen_up);
    pen_values[0] = "down";
    assert(wifi_pen_parse_params(1, pen_names, pen_values, &pen_up));
    assert(!pen_up);
    pen_values[0] = "toggle";
    assert(!wifi_pen_parse_params(1, pen_names, pen_values, &pen_up));
}

static void test_single_slot_queue(void) {
    wifi_jog_request_t slot = {0};
    wifi_jog_request_t in = {.axis = 'X', .direction = 1,
                             .distance_mm = 10.0f, .feed_mm_min = 500u};
    wifi_jog_request_t out = {0};
    bool pending = false;
    assert(wifi_jog_queue_offer(&slot, &pending, &in));
    assert(!wifi_jog_queue_offer(&slot, &pending, &in));
    assert(wifi_jog_queue_take(&slot, &pending, &out));
    assert(out.axis == 'X' && out.distance_mm == 10.0f);
    assert(!wifi_jog_queue_take(&slot, &pending, &out));
}

static void test_status_and_ssid(void) {
    wifi_jog_status_t status = {
        .ready = true, .busy = false, .armed = true, .outputs = false,
        .active_axes_only = true, .microsteps = 1u, .capture_ms = 100u,
        .active_mask = 2u, .x_angle = 0, .y_angle = 64,
        .x_phase = "A+", .y_phase = "D-",
        .clients = 1u, .x = 0.625f, .y = 0.0f, .z = 1.0f,
        .pen_pulse_us = 1400u, .pen_up = true,
        .last_result = "ok",
    };
    char json[384];
    assert(wifi_jog_format_status(&status, json, sizeof(json)) > 0);
    assert(strstr(json, "\"ready\":true") != NULL);
    assert(strstr(json, "\"x\":0.6250") != NULL);
    assert(strstr(json, "\"activeOnly\":true") != NULL);
    assert(strstr(json, "\"microsteps\":1") != NULL);
    assert(strstr(json, "\"captureMs\":100") != NULL);
    assert(strstr(json, "\"activeMask\":2") != NULL);
    assert(strstr(json, "\"yPhase\":\"D-\"") != NULL);
    assert(strstr(json, "\"clients\":1") != NULL);
    assert(strstr(json, "\"z\":1.0000") != NULL);
    assert(strstr(json, "\"penPulseUs\":1400") != NULL);
    assert(strstr(json, "\"penUp\":true") != NULL);
    assert(strstr(json, "\"lastResult\":\"ok\"") != NULL);

    char ssid[32];
    assert(wifi_jog_make_ssid("E6614104037B8123", ssid, sizeof(ssid)) > 0);
    assert(strcmp(ssid, "PlanarStepper-8123") == 0);
}

int main(void) {
    test_parameters_and_gcode();
    test_single_slot_queue();
    test_status_and_ssid();
    puts("wifi_jog_protocol tests passed");
    return 0;
}
