#pragma once

#include <stdbool.h>

#include "gcode.h"
#include "wifi_jog_protocol.h"

bool wifi_control_init(void);
bool wifi_control_take_jog(wifi_jog_request_t *request);
bool wifi_control_take_mode(wifi_drive_request_t *request);
bool wifi_control_take_pen(bool *pen_up);
bool wifi_control_take_stop(void);
void wifi_control_set_running(void);
void wifi_control_set_result(const char *result);
void wifi_control_update_status(const controller_state_t *state);
const char *wifi_control_ssid(void);
int wifi_control_ap_client_count(void);
