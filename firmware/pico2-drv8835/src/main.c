#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "config.h"
#include "gcode.h"
#include "planner.h"
#if MOTION_BACKEND_DRV8835
#include "current_monitor.h"
#include "position_sensors.h"
#include "servo_pen.h"
#endif
#include "stepper.h"

static void print_banner(void) {
    printf("\r\n%s %s ready\r\n", FW_NAME, FW_VERSION);
    printf("[MSG:'$' not implemented; PlotterFlow GRBL/FluidNC subset active]\r\n");
}

int main(void) {
    stepper_safe_init();
#if MOTION_BACKEND_DRV8835
    servo_pen_safe_init();
    current_monitor_init();
    position_sensors_init();
#endif
    stdio_init_all();
    sleep_ms(1200);

    print_banner();
#if MOTION_BACKEND_DRV8835
    printf("[SAFE: DRV8835 IN/IN initialized LOW before USB startup]\r\n");
    printf("[SAFE: pen servo GP%u initialized UP at %uus]\r\n",
           PIN_PEN_SERVO_PWM, servo_pen_current_pulse_us());
#else
    printf("[SAFE: X/Y EN initialized HIGH (disabled) before USB startup]\r\n");
#endif

    controller_state_t state;
    gcode_state_init(&state);
    planner_init();
#if !MOTION_BACKEND_DRV8835
    tmc2209_bus_init();
    const bool x_ready = tmc2209_init_driver(TMC_ADDR_X, DEFAULT_X_RUN_CURRENT_MA,
                                             DEFAULT_X_HOLD_CURRENT_MA, DEFAULT_MICROSTEPS);
    const bool y_ready = tmc2209_init_driver(TMC_ADDR_Y, DEFAULT_Y_RUN_CURRENT_MA,
                                             DEFAULT_Y_HOLD_CURRENT_MA, DEFAULT_MICROSTEPS);
    printf("[TMC INIT: X=%s Y=%s; EN remains disabled until M17/motion]\r\n",
           x_ready ? "ok" : "failed", y_ready ? "ok" : "failed");
#else
    printf("[DRV8835 INIT: X=GP19/20/21/22 Y=GP15/16/17/18 "
           "Z_SERVO=GP%u; outputs remain HiZ until motion]\r\n",
           PIN_PEN_SERVO_PWM);
    printf("[CURRENT INIT: optional X-A sensor=GP%u/ADC%u; "
           "missing sensor does not block motion]\r\n",
           PIN_CURRENT_SENSOR_XA_GPIO, PIN_CURRENT_SENSOR_XA_ADC_INPUT);
    printf("[POSITION INIT: optional AS5600 J1=I2C0 GP%u/GP%u "
           "J2=I2C1 GP%u/GP%u; missing sensors do not block motion]\r\n",
           PIN_AS5600_J1_SDA, PIN_AS5600_J1_SCL,
           PIN_AS5600_J2_SDA, PIN_AS5600_J2_SCL);
#endif

    char line[160];
    size_t length = 0;

    while (true) {
        int ch = getchar_timeout_us(1000);
        if (ch == PICO_ERROR_TIMEOUT) {
            planner_service();
            continue;
        }

        if (ch == '?' || ch == '!' || ch == '~' || ch == 0x18 || ch == 0x85) {
            gcode_process_realtime(ch, &state);
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            if (length > 0) {
                line[length] = '\0';
                gcode_process_line(line, &state);
                length = 0;
            }
            continue;
        }

        if (length < sizeof(line) - 1) {
            line[length++] = (char)ch;
        } else {
            length = 0;
            printf("error: line too long\r\n");
        }
    }
}
