#include <stdio.h>

#include "pico/stdlib.h"

#include "config.h"
#include "current_monitor.h"
#include "gcode.h"
#include "planner.h"
#include "position_sensors.h"
#include "servo_pen.h"
#include "stepper.h"
#include "wifi_control.h"

static void print_banner(void) {
    printf("\r\n%s %s ready\r\n", FW_NAME, FW_VERSION);
    printf("[SAFE: DRV8835 GP15..22 initialized LOW before USB/Wi-Fi startup]\r\n");
}

static void service_wifi(controller_state_t *state) {
    static uint32_t next_client_check_ms = 0;
    static int previous_client_count = -1;
    if (wifi_control_take_stop()) {
        planner_cancel_jog(state);
        char pen_up_command[] = "G0 Z1";
        gcode_process_line(pen_up_command, state);
        wifi_control_set_result("stopped");
        printf("[MSG:Wi-Fi stop; DRV8835 outputs HiZ; pen up]\r\n");
    }

    bool pen_up = false;
    if (wifi_control_take_pen(&pen_up)) {
        char pen_command[16];
        snprintf(pen_command, sizeof(pen_command),
                 pen_up ? "G0 Z1" : "G1 Z0");
        wifi_control_set_running();
        gcode_process_line(pen_command, state);
        wifi_control_set_result(pen_up ? "pen_up" : "pen_down");
    }

    wifi_drive_request_t drive_request;
    if (wifi_control_take_mode(&drive_request)) {
        char disable_command[] = "M18";
        char mode_command[48];
        snprintf(mode_command, sizeof(mode_command), "M980 U%u A%u C%u",
                 drive_request.microsteps,
                 drive_request.active_axes_only ? 1u : 0u,
                 drive_request.capture_ms);
        gcode_process_line(disable_command, state);
        gcode_process_line(mode_command, state);
        wifi_control_set_result("configured");
    }

    wifi_jog_request_t request;
    if (wifi_control_take_jog(&request)) {
        char command[64];
        const int length = wifi_jog_build_gcode(&request, command, sizeof(command));
        if (length <= 0 || (size_t)length >= sizeof(command)) {
            wifi_control_set_result("error");
        } else {
            wifi_control_set_running();
            printf("[MSG:Wi-Fi jog %s]\r\n", command);
            gcode_process_line(command, state);
            if (wifi_control_take_stop()) {
                planner_cancel_jog(state);
                wifi_control_set_result("stopped");
            } else {
                wifi_control_set_result("ok");
            }
        }
    }
    planner_service();
    wifi_control_update_status(state);

    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if ((int32_t)(now_ms - next_client_check_ms) >= 0) {
        const int clients = wifi_control_ap_client_count();
        if (clients >= 0 && clients != previous_client_count) {
            printf("[WIFI AP: clients=%d]\r\n", clients);
            previous_client_count = clients;
        }
        next_client_check_ms = now_ms + 1000u;
    }
}

int main(void) {
    stepper_safe_init();
    servo_pen_safe_init();
    current_monitor_init();
    position_sensors_init();
    stdio_init_all();
    sleep_ms(1200);
    print_banner();

    controller_state_t state;
    gcode_state_init(&state);
    planner_init();
    printf("[DRV8835 INIT: X=GP19/20/21/22 Y=GP15/16/17/18 "
           "Z_SERVO=GP%u UP=%uus; outputs HiZ]\r\n",
           PIN_PEN_SERVO_PWM, servo_pen_current_pulse_us());
    printf("[CURRENT INIT: optional X-A sensor=GP%u/ADC%u; "
           "missing sensor does not block motion]\r\n",
           PIN_CURRENT_SENSOR_XA_GPIO, PIN_CURRENT_SENSOR_XA_ADC_INPUT);
    printf("[POSITION INIT: optional AS5600 J1=I2C0 GP%u/GP%u "
           "J2=I2C1 GP%u/GP%u; missing sensors do not block motion]\r\n",
           PIN_AS5600_J1_SDA, PIN_AS5600_J1_SCL,
           PIN_AS5600_J2_SDA, PIN_AS5600_J2_SCL);

    if (wifi_control_init()) {
        printf("[WIFI AP: SSID=%s password=planar-stepper URL=http://192.168.4.1/]\r\n",
               wifi_control_ssid());
    } else {
        printf("[WIFI ERROR: AP unavailable; USB G-code remains available]\r\n");
    }
    wifi_control_update_status(&state);

    char line[160];
    size_t length = 0;
    while (true) {
        service_wifi(&state);
        const int ch = getchar_timeout_us(1000);
        if (ch == PICO_ERROR_TIMEOUT) {
            continue;
        }
        if (ch == '?' || ch == '!' || ch == '~' || ch == 0x18 || ch == 0x85) {
            gcode_process_realtime(ch, &state);
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (length > 0u) {
                line[length] = '\0';
                gcode_process_line(line, &state);
                length = 0u;
            }
            continue;
        }
        if (length < sizeof(line) - 1u) {
            line[length++] = (char)ch;
        } else {
            length = 0u;
            printf("error: line too long\r\n");
        }
    }
}
