#include "servo_pen.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "config.h"

#define SERVO_COUNTER_HZ 1000000u
#define SERVO_PWM_WRAP ((SERVO_COUNTER_HZ / SERVO_PEN_PWM_FREQUENCY_HZ) - 1u)

static servo_pen_config_t servo_config = {
    .up_pulse_us = SERVO_PEN_DEFAULT_UP_US,
    .down_pulse_us = SERVO_PEN_DEFAULT_DOWN_US,
    .settle_ms = SERVO_PEN_DEFAULT_SETTLE_MS,
    .z_threshold = SERVO_PEN_DEFAULT_Z_THRESHOLD,
};
static unsigned int current_pulse_us = SERVO_PEN_DEFAULT_UP_US;
static bool current_is_up = true;

static void apply_pulse(unsigned int pulse_us) {
    pwm_set_gpio_level(PIN_PEN_SERVO_PWM, pulse_us);
    current_pulse_us = pulse_us;
}

void servo_pen_safe_init(void) {
    gpio_init(PIN_PEN_SERVO_PWM);
    gpio_put(PIN_PEN_SERVO_PWM, false);
    gpio_set_dir(PIN_PEN_SERVO_PWM, GPIO_OUT);

    gpio_set_function(PIN_PEN_SERVO_PWM, GPIO_FUNC_PWM);
    const unsigned int slice = pwm_gpio_to_slice_num(PIN_PEN_SERVO_PWM);
    pwm_config pwm_cfg = pwm_get_default_config();
    const float divider =
        (float)clock_get_hz(clk_sys) / (float)SERVO_COUNTER_HZ;
    pwm_config_set_clkdiv(&pwm_cfg, divider);
    pwm_config_set_wrap(&pwm_cfg, SERVO_PWM_WRAP);
    pwm_init(slice, &pwm_cfg, true);
    current_is_up = true;
    apply_pulse(servo_config.up_pulse_us);
}

bool servo_pen_configure(const servo_pen_config_t *config) {
    if (!servo_pen_config_valid(config)) {
        return false;
    }
    servo_config = *config;
    apply_pulse(current_is_up ? servo_config.up_pulse_us
                              : servo_config.down_pulse_us);
    return true;
}

void servo_pen_get_config(servo_pen_config_t *config) {
    if (config) {
        *config = servo_config;
    }
}

bool servo_pen_set_pulse_us(unsigned int pulse_us) {
    if (pulse_us < SERVO_PEN_MIN_PULSE_US ||
        pulse_us > SERVO_PEN_MAX_PULSE_US) {
        return false;
    }
    apply_pulse(pulse_us);
    if (pulse_us == servo_config.up_pulse_us) {
        current_is_up = true;
    } else if (pulse_us == servo_config.down_pulse_us) {
        current_is_up = false;
    }
    return true;
}

bool servo_pen_move_up(bool *changed) {
    const bool did_change =
        !current_is_up || current_pulse_us != servo_config.up_pulse_us;
    current_is_up = true;
    apply_pulse(servo_config.up_pulse_us);
    if (changed) {
        *changed = did_change;
    }
    return true;
}

bool servo_pen_move_down(bool *changed) {
    const bool did_change =
        current_is_up || current_pulse_us != servo_config.down_pulse_us;
    current_is_up = false;
    apply_pulse(servo_config.down_pulse_us);
    if (changed) {
        *changed = did_change;
    }
    return true;
}

bool servo_pen_set_z(float z, bool *changed) {
    return servo_pen_z_is_up(z, servo_config.z_threshold)
               ? servo_pen_move_up(changed)
               : servo_pen_move_down(changed);
}

unsigned int servo_pen_current_pulse_us(void) {
    return current_pulse_us;
}

bool servo_pen_is_up(void) {
    return current_is_up;
}
