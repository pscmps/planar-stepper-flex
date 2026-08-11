#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"

#define FW_NAME "pico2-gcode-drv8835-xy-test"
#define FW_VERSION "0.4.0"

#define PIN_C_IN1 15u
#define PIN_C_IN2 16u
#define PIN_D_IN1 17u
#define PIN_D_IN2 18u
#define PIN_A_IN1 19u
#define PIN_A_IN2 20u
#define PIN_B_IN1 21u
#define PIN_B_IN2 22u

#define PWM_FREQUENCY_HZ 20000u
#define PWM_COUNTER_HZ 10000000u
#define PWM_WRAP ((PWM_COUNTER_HZ / PWM_FREQUENCY_HZ) - 1u)
#define ELECTRICAL_CYCLE_UNITS 256
#define QUADRANT_UNITS 64
#define TWO_PI_F 6.28318530717958647692f

static bool output_active = false;
static unsigned int active_axis = 0u;
static unsigned int active_phase = 0u;
static unsigned int active_duty = 0u;
static int axis_angle[2] = {0, 0};

enum {
    AXIS_X = 0u,
    AXIS_Y = 1u,
};

static const unsigned int phase_pins[2][4] = {
    {PIN_A_IN1, PIN_B_IN2, PIN_A_IN2, PIN_B_IN1},
    {PIN_C_IN1, PIN_D_IN2, PIN_C_IN2, PIN_D_IN1},
};

static const unsigned int driver_pins[8] = {
    PIN_A_IN1, PIN_A_IN2, PIN_B_IN1, PIN_B_IN2,
    PIN_C_IN1, PIN_C_IN2, PIN_D_IN1, PIN_D_IN2,
};

static const char *axis_names[2] = {"X", "Y"};
static const char *phase_names[2][4] = {
    {"A+", "B-", "A-", "B+"},
    {"C+", "D-", "C-", "D+"},
};

static const unsigned int coil_positive_pins[2][2] = {
    {PIN_A_IN1, PIN_B_IN1},
    {PIN_C_IN1, PIN_D_IN1},
};

static const unsigned int coil_negative_pins[2][2] = {
    {PIN_A_IN2, PIN_B_IN2},
    {PIN_C_IN2, PIN_D_IN2},
};

static void safe_gpio_init(void) {
    const unsigned int previous_drv8835_pins[] = {11u, 12u, 13u, 14u};
    for (size_t i = 0;
         i < sizeof(previous_drv8835_pins) / sizeof(previous_drv8835_pins[0]);
         i++) {
        gpio_init(previous_drv8835_pins[i]);
        gpio_put(previous_drv8835_pins[i], 0);
        gpio_set_dir(previous_drv8835_pins[i], GPIO_OUT);
    }

    for (size_t i = 0; i < sizeof(driver_pins) / sizeof(driver_pins[0]); i++) {
        gpio_init(driver_pins[i]);
        gpio_disable_pulls(driver_pins[i]);
        gpio_set_drive_strength(driver_pins[i], GPIO_DRIVE_STRENGTH_12MA);
        gpio_put(driver_pins[i], 0);
        gpio_set_dir(driver_pins[i], GPIO_OUT);
    }
    output_active = false;
}

static void pwm_init_hardware(void) {
    const float divider = (float)clock_get_hz(clk_sys) / (float)PWM_COUNTER_HZ;
    bool configured_slices[NUM_PWM_SLICES] = {false};

    for (size_t i = 0; i < sizeof(driver_pins) / sizeof(driver_pins[0]); i++) {
        const unsigned int slice = pwm_gpio_to_slice_num(driver_pins[i]);
        pwm_set_gpio_level(driver_pins[i], 0u);
        if (!configured_slices[slice]) {
            pwm_config config = pwm_get_default_config();
            pwm_config_set_clkdiv(&config, divider);
            pwm_config_set_wrap(&config, PWM_WRAP);
            pwm_init(slice, &config, true);
            configured_slices[slice] = true;
        }
    }
}

static void all_off(void) {
    for (size_t i = 0; i < sizeof(driver_pins) / sizeof(driver_pins[0]); i++) {
        gpio_put(driver_pins[i], 0);
        gpio_set_function(driver_pins[i], GPIO_FUNC_SIO);
        gpio_set_dir(driver_pins[i], GPIO_OUT);
    }
    output_active = false;
    active_duty = 0u;
}

static void set_pin_duty(unsigned int pin, unsigned int duty_percent) {
    if (duty_percent == 0u) {
        return;
    }
    if (duty_percent == 100u) {
        gpio_set_function(pin, GPIO_FUNC_SIO);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 1);
    } else {
        const uint32_t level =
            ((uint32_t)(PWM_WRAP + 1u) * duty_percent + 99u) / 100u;
        pwm_set_gpio_level(pin, level);
        gpio_set_function(pin, GPIO_FUNC_PWM);
    }
}

static void set_phase(unsigned int axis, unsigned int phase,
                      unsigned int duty_percent) {
    all_off();
    const unsigned int pin = phase_pins[axis][phase];
    set_pin_duty(pin, duty_percent);
    output_active = true;
    active_axis = axis;
    active_phase = phase;
    active_duty = duty_percent;
    axis_angle[axis] = (int)phase * QUADRANT_UNITS;
    sleep_us(10);
}

static int wrap_angle(int angle) {
    angle %= ELECTRICAL_CYCLE_UNITS;
    return angle < 0 ? angle + ELECTRICAL_CYCLE_UNITS : angle;
}

static void set_axis_angle(unsigned int axis, int angle, unsigned int peak_duty,
                           int *coil_0_duty, int *coil_1_duty) {
    all_off();
    angle = wrap_angle(angle);
    const float radians = TWO_PI_F * (float)angle / (float)ELECTRICAL_CYCLE_UNITS;
    const int duty_0 = (int)lroundf(cosf(radians) * (float)peak_duty);
    const int duty_1 = (int)lroundf(-sinf(radians) * (float)peak_duty);
    const unsigned int pin_0 = duty_0 >= 0 ? coil_positive_pins[axis][0]
                                          : coil_negative_pins[axis][0];
    const unsigned int pin_1 = duty_1 >= 0 ? coil_positive_pins[axis][1]
                                          : coil_negative_pins[axis][1];
    set_pin_duty(pin_0, (unsigned int)abs(duty_0));
    set_pin_duty(pin_1, (unsigned int)abs(duty_1));
    axis_angle[axis] = angle;
    active_axis = axis;
    active_phase = (unsigned int)((angle + (QUADRANT_UNITS / 2)) /
                                  QUADRANT_UNITS) & 3u;
    active_duty = peak_duty;
    output_active = true;
    *coil_0_duty = duty_0;
    *coil_1_duty = duty_1;
    sleep_us(10);
}

static void report_pin_state(unsigned int pin) {
    printf("pin=GP%u func=%u dir=%s latch=%u pad=%u", pin,
           (unsigned int)gpio_get_function(pin),
           gpio_is_dir_out(pin) ? "OUT" : "IN", gpio_get_out_level(pin),
           gpio_get(pin));
}

static bool word_value(const char *line, char letter, float *value) {
    for (const char *p = line; *p; p++) {
        if (*p == letter) {
            char *end = NULL;
            const float parsed = strtof(p + 1, &end);
            if (end != p + 1) {
                *value = parsed;
                return true;
            }
        }
    }
    return false;
}

static void normalize(char *line) {
    char *out = line;
    bool paren = false;
    for (char *in = line; *in; in++) {
        if (*in == ';') {
            break;
        }
        if (*in == '(') {
            paren = true;
        } else if (*in == ')') {
            paren = false;
        } else if (!paren && !isspace((unsigned char)*in)) {
            *out++ = (char)toupper((unsigned char)*in);
        }
    }
    *out = '\0';
}

static bool parse_uint_word(const char *line, char letter, unsigned int minimum,
                            unsigned int maximum, unsigned int *result) {
    float value = 0.0f;
    if (!word_value(line, letter, &value)) {
        return false;
    }
    const long rounded = lroundf(value);
    if (rounded < (long)minimum || rounded > (long)maximum ||
        fabsf(value - (float)rounded) > 0.001f) {
        return false;
    }
    *result = (unsigned int)rounded;
    return true;
}

static bool parse_int_word(const char *line, char letter, int minimum,
                           int maximum, int *result) {
    float value = 0.0f;
    if (!word_value(line, letter, &value)) {
        return false;
    }
    const long rounded = lroundf(value);
    if (rounded < (long)minimum || rounded > (long)maximum || rounded == 0 ||
        fabsf(value - (float)rounded) > 0.001f) {
        return false;
    }
    *result = (int)rounded;
    return true;
}

static bool valid_microsteps(unsigned int microsteps) {
    return microsteps == 1u || microsteps == 2u || microsteps == 4u ||
           microsteps == 8u || microsteps == 16u;
}

static void report_status(void) {
    if (output_active) {
        printf("<Run|Axis:%s|Phase:%s|Duty:%u|PWM:%u>\r\n",
               axis_names[active_axis], phase_names[active_axis][active_phase],
               active_duty, PWM_FREQUENCY_HZ);
    } else {
        printf("<Idle|Axis:XY|Outputs:HiZ|PWM:%u>\r\n", PWM_FREQUENCY_HZ);
    }
}

static bool wait_with_realtime_stop(unsigned int dwell_ms) {
    const absolute_time_t deadline = make_timeout_time_ms(dwell_ms);
    while (!time_reached(deadline)) {
        const int ch = getchar_timeout_us(1000);
        if (ch == '!' || ch == 0x18 || ch == 0x85) {
            all_off();
            printf("[MSG:DRV8835 realtime stop; outputs=HiZ]\r\n");
            return false;
        }
    }
    return true;
}

static bool dwell_with_realtime_stop(unsigned int dwell_ms) {
    const bool completed = wait_with_realtime_stop(dwell_ms);
    all_off();
    return completed;
}

static void process_line(char *line) {
    normalize(line);
    if (*line == '\0') {
        printf("ok\r\n");
        return;
    }
    if (strcmp(line, "M115") == 0) {
        printf("FIRMWARE_NAME:%s FIRMWARE_VERSION:%s MACHINE:Pico2_DRV8835_XY_Test\r\n",
               FW_NAME, FW_VERSION);
        printf("ok\r\n");
        return;
    }
    if (strcmp(line, "M18") == 0 || strcmp(line, "M84") == 0) {
        all_off();
        printf("[MSG:DRV8835 XY outputs=HiZ X=GP%u/GP%u/GP%u/GP%u Y=GP%u/GP%u/GP%u/GP%u]\r\n",
               PIN_A_IN1, PIN_A_IN2, PIN_B_IN1, PIN_B_IN2, PIN_C_IN1,
               PIN_C_IN2, PIN_D_IN1, PIN_D_IN2);
        printf("ok\r\n");
        return;
    }
    if (strncmp(line, "M974", 4) == 0) {
        unsigned int phase = 0u;
        unsigned int duty = 0u;
        unsigned int dwell_ms = 0u;
        const bool use_x = strchr(line + 4, 'X') != NULL;
        const bool use_y = strchr(line + 4, 'Y') != NULL;
        if (use_x == use_y || !parse_uint_word(line, 'S', 0u, 3u, &phase) ||
            !parse_uint_word(line, 'D', 1u, 100u, &duty) ||
            !parse_uint_word(line, 'P', 50u, 2000u, &dwell_ms)) {
            all_off();
            printf("error: M974 requires X|Y S0..3 D1..100 P50..2000\r\n");
            return;
        }
        const unsigned int axis = use_x ? AXIS_X : AXIS_Y;
        set_phase(axis, phase, duty);
        printf("[MSG:M974 axis=%s phase=%s ", axis_names[axis],
               phase_names[axis][phase]);
        report_pin_state(phase_pins[axis][phase]);
        printf(" drive=%s duty=%u%% pwm=%uHz state=ON dwell_ms=%u]\r\n",
               duty == 100u ? "GPIO_DC" : "PWM", duty, PWM_FREQUENCY_HZ,
               dwell_ms);
        const bool completed = dwell_with_realtime_stop(dwell_ms);
        printf("[MSG:M974 axis=%s phase=%s state=OFF outputs=HiZ result=%s]\r\n",
               axis_names[axis], phase_names[axis][phase],
               completed ? "ok" : "stopped");
        printf("ok\r\n");
        return;
    }
    if (strncmp(line, "M975", 4) == 0) {
        unsigned int cycles = 0u;
        unsigned int duty = 0u;
        unsigned int dwell_ms = 0u;
        unsigned int x_cycles = 0u;
        unsigned int y_cycles = 0u;
        const bool use_x = parse_uint_word(line, 'X', 1u, 10u, &x_cycles);
        const bool use_y = parse_uint_word(line, 'Y', 1u, 10u, &y_cycles);
        if (use_x == use_y ||
            !parse_uint_word(line, 'D', 1u, 100u, &duty) ||
            !parse_uint_word(line, 'P', 10u, 1000u, &dwell_ms)) {
            all_off();
            printf("error: M975 requires X1..10|Y1..10 D1..100 P10..1000\r\n");
            return;
        }
        const unsigned int axis = use_x ? AXIS_X : AXIS_Y;
        cycles = use_x ? x_cycles : y_cycles;
        bool completed = true;
        for (unsigned int cycle = 0u; cycle < cycles && completed; cycle++) {
            for (unsigned int phase = 0u; phase < 4u; phase++) {
                set_phase(axis, phase, duty);
                printf("[MSG:M975 axis=%s cycle=%u/%u phase=%s ",
                       axis_names[axis], cycle + 1u, cycles,
                       phase_names[axis][phase]);
                report_pin_state(phase_pins[axis][phase]);
                printf(" drive=%s duty=%u%% dwell_ms=%u]\r\n",
                       duty == 100u ? "GPIO_DC" : "PWM", duty, dwell_ms);
                if (!dwell_with_realtime_stop(dwell_ms)) {
                    completed = false;
                    break;
                }
            }
        }
        printf("[MSG:M975 axis=%s cycles=%u result=%s outputs=HiZ]\r\n",
               axis_names[axis], cycles, completed ? "ok" : "stopped");
        printf("ok\r\n");
        return;
    }
    if (strncmp(line, "M977", 4) == 0) {
        int x_transitions = 0;
        int y_transitions = 0;
        unsigned int microsteps = 0u;
        unsigned int peak_duty = 0u;
        unsigned int dwell_ms = 0u;
        const bool use_x = parse_int_word(line, 'X', -16, 16, &x_transitions);
        const bool use_y = parse_int_word(line, 'Y', -16, 16, &y_transitions);
        if (use_x == use_y ||
            !parse_uint_word(line, 'U', 1u, 16u, &microsteps) ||
            !valid_microsteps(microsteps) ||
            !parse_uint_word(line, 'D', 1u, 100u, &peak_duty) ||
            !parse_uint_word(line, 'P', 5u, 500u, &dwell_ms)) {
            all_off();
            printf("error: M977 requires X-16..16|Y-16..16 U1|2|4|8|16 D1..100 P5..500\r\n");
            return;
        }
        const unsigned int axis = use_x ? AXIS_X : AXIS_Y;
        const int transitions = use_x ? x_transitions : y_transitions;
        const int direction = transitions > 0 ? 1 : -1;
        const unsigned int total_steps =
            (unsigned int)abs(transitions) * microsteps;
        const int angle_step = direction * QUADRANT_UNITS / (int)microsteps;
        bool completed = true;
        int duty_0 = 0;
        int duty_1 = 0;

        set_axis_angle(axis, axis_angle[axis], peak_duty, &duty_0, &duty_1);
        printf("[MSG:M977 axis=%s start_angle=%d coil0=%d coil1=%d transitions=%d microsteps=%u peak=%u%% dwell_ms=%u]\r\n",
               axis_names[axis], axis_angle[axis], duty_0, duty_1, transitions,
               microsteps, peak_duty, dwell_ms);
        if (!wait_with_realtime_stop(dwell_ms)) {
            completed = false;
        }
        for (unsigned int step = 0u; step < total_steps && completed; step++) {
            const int next_angle = axis_angle[axis] + angle_step;
            set_axis_angle(axis, next_angle, peak_duty, &duty_0, &duty_1);
            printf("[MSG:M977 axis=%s step=%u/%u angle=%d coil0=%d coil1=%d]\r\n",
                   axis_names[axis], step + 1u, total_steps, axis_angle[axis],
                   duty_0, duty_1);
            if (!wait_with_realtime_stop(dwell_ms)) {
                completed = false;
            }
        }
        all_off();
        printf("[MSG:M977 axis=%s transitions=%d result=%s final_angle=%d outputs=HiZ]\r\n",
               axis_names[axis], transitions, completed ? "ok" : "stopped",
               axis_angle[axis]);
        printf("ok\r\n");
        return;
    }
    if (strncmp(line, "M978", 4) == 0) {
        int x_steps = 0;
        int y_steps = 0;
        unsigned int microsteps = 0u;
        unsigned int peak_duty = 0u;
        unsigned int dwell_ms = 0u;
        const bool use_x = parse_int_word(line, 'X', -128, 128, &x_steps);
        const bool use_y = parse_int_word(line, 'Y', -128, 128, &y_steps);
        if (use_x == use_y ||
            !parse_uint_word(line, 'U', 1u, 16u, &microsteps) ||
            !valid_microsteps(microsteps) ||
            !parse_uint_word(line, 'D', 1u, 100u, &peak_duty) ||
            !parse_uint_word(line, 'P', 5u, 500u, &dwell_ms)) {
            all_off();
            printf("error: M978 requires X-128..128|Y-128..128 U1|2|4|8|16 D1..100 P5..500\r\n");
            return;
        }
        const unsigned int axis = use_x ? AXIS_X : AXIS_Y;
        const int requested_steps = use_x ? x_steps : y_steps;
        const int direction = requested_steps > 0 ? 1 : -1;
        const unsigned int total_steps = (unsigned int)abs(requested_steps);
        const int angle_step = direction * QUADRANT_UNITS / (int)microsteps;
        bool completed = true;
        int duty_0 = 0;
        int duty_1 = 0;

        set_axis_angle(axis, axis_angle[axis], peak_duty, &duty_0, &duty_1);
        printf("[MSG:M978 axis=%s start_angle=%d coil0=%d coil1=%d steps=%d microsteps=%u peak=%u%% dwell_ms=%u]\r\n",
               axis_names[axis], axis_angle[axis], duty_0, duty_1,
               requested_steps, microsteps, peak_duty, dwell_ms);
        if (!wait_with_realtime_stop(dwell_ms)) {
            completed = false;
        }
        for (unsigned int step = 0u; step < total_steps && completed; step++) {
            const int next_angle = axis_angle[axis] + angle_step;
            set_axis_angle(axis, next_angle, peak_duty, &duty_0, &duty_1);
            printf("[MSG:M978 axis=%s step=%u/%u angle=%d coil0=%d coil1=%d]\r\n",
                   axis_names[axis], step + 1u, total_steps, axis_angle[axis],
                   duty_0, duty_1);
            if (!wait_with_realtime_stop(dwell_ms)) {
                completed = false;
            }
        }
        all_off();
        printf("[MSG:M978 axis=%s steps=%d result=%s final_angle=%d outputs=HiZ]\r\n",
               axis_names[axis], requested_steps,
               completed ? "ok" : "stopped", axis_angle[axis]);
        printf("ok\r\n");
        return;
    }
    printf("error: unsupported command\r\n");
}

int main(void) {
    safe_gpio_init();
    stdio_init_all();
    sleep_ms(1200);
    pwm_init_hardware();
    all_off();

    printf("\r\n%s %s ready\r\n", FW_NAME, FW_VERSION);
    printf("[SAFE: DRV8835 IN/IN inputs initialized LOW; outputs HiZ]\r\n");
    printf("[PIN X: A AIN1=GP%u(physical25) AIN2=GP%u(physical26); "
           "B BIN1=GP%u(physical27) BIN2=GP%u(physical29)]\r\n",
           PIN_A_IN1, PIN_A_IN2, PIN_B_IN1, PIN_B_IN2);
    printf("[PIN Y: C AIN1=GP%u(physical20) AIN2=GP%u(physical21); "
           "D BIN1=GP%u(physical22) BIN2=GP%u(physical24); MODE=GND]\r\n",
           PIN_C_IN1, PIN_C_IN2, PIN_D_IN1, PIN_D_IN2);

    char line[128];
    size_t length = 0u;
    while (true) {
        const int ch = getchar_timeout_us(1000);
        if (ch == PICO_ERROR_TIMEOUT) {
            continue;
        }
        if (ch == '?' ) {
            report_status();
            continue;
        }
        if (ch == '!' || ch == 0x18 || ch == 0x85) {
            all_off();
            printf("[MSG:DRV8835 realtime stop; outputs=HiZ]\r\n");
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (length > 0u) {
                line[length] = '\0';
                process_line(line);
                length = 0u;
            }
            continue;
        }
        if (length < sizeof(line) - 1u) {
            line[length++] = (char)ch;
        } else {
            length = 0u;
            all_off();
            printf("error: line too long; outputs HiZ\r\n");
        }
    }
}
