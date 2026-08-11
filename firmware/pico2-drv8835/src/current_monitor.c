#include "current_monitor.h"

#include <stdio.h>

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#include "config.h"
#include "current_monitor_math.h"

typedef struct {
    uint16_t raw;
    uint16_t zero;
    int32_t sum;
    int32_t absolute_sum;
    int16_t minimum;
    int16_t maximum;
    uint16_t samples;
    current_monitor_snapshot_t published;
    uint32_t next_sample_us;
    uint32_t window_started_ms;
} monitor_state_t;

static monitor_state_t monitor = {
    .zero = 2048u,
};

static bool raw_is_present(uint16_t raw) {
    return raw >= CURRENT_SENSOR_PRESENT_RAW_MIN &&
           raw <= CURRENT_SENSOR_PRESENT_RAW_MAX;
}

static uint16_t read_sensor(void) {
    adc_select_input(PIN_CURRENT_SENSOR_XA_ADC_INPUT);
    return adc_read();
}

static void publish_window(void) {
    monitor.published.present = raw_is_present(monitor.raw);
    monitor.published.raw = monitor.raw;
    monitor.published.zero = monitor.zero;
    if (monitor.samples > 0u && monitor.published.present) {
        const float scale = 3.3f /
            (4095.0f * CURRENT_SENSOR_SENSITIVITY_V_PER_A);
        monitor.published.average_a =
            scale * (float)monitor.sum / (float)monitor.samples;
        monitor.published.absolute_average_a =
            scale * (float)monitor.absolute_sum / (float)monitor.samples;
        monitor.published.minimum_a = scale * (float)monitor.minimum;
        monitor.published.maximum_a = scale * (float)monitor.maximum;
    } else {
        monitor.published.average_a = 0.0f;
        monitor.published.absolute_average_a = 0.0f;
        monitor.published.minimum_a = 0.0f;
        monitor.published.maximum_a = 0.0f;
    }
    monitor.sum = 0;
    monitor.absolute_sum = 0;
    monitor.minimum = 0;
    monitor.maximum = 0;
    monitor.samples = 0u;
}

void current_monitor_init(void) {
    adc_init();
    adc_gpio_init(PIN_CURRENT_SENSOR_XA_GPIO);
    gpio_pull_down(PIN_CURRENT_SENSOR_XA_GPIO);
    monitor.next_sample_us = time_us_32();
    monitor.window_started_ms = to_ms_since_boot(get_absolute_time());
    (void)current_monitor_calibrate();
}

void current_monitor_service(void) {
    const uint32_t now_us = time_us_32();
    if ((int32_t)(now_us - monitor.next_sample_us) >= 0) {
        monitor.next_sample_us = now_us + CURRENT_SENSOR_SAMPLE_INTERVAL_US;
        monitor.raw = read_sensor();
        const int delta = (int)monitor.raw - (int)monitor.zero;
        monitor.sum += delta;
        monitor.absolute_sum += delta < 0 ? -delta : delta;
        if (monitor.samples == 0u) {
            monitor.minimum = (int16_t)delta;
            monitor.maximum = (int16_t)delta;
        } else {
            if (delta < monitor.minimum) {
                monitor.minimum = (int16_t)delta;
            }
            if (delta > monitor.maximum) {
                monitor.maximum = (int16_t)delta;
            }
        }
        monitor.samples++;
    }
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (now_ms - monitor.window_started_ms >= CURRENT_SENSOR_REPORT_INTERVAL_MS) {
        publish_window();
        monitor.window_started_ms = now_ms;
    }
}

bool current_monitor_calibrate(void) {
    uint32_t sum = 0u;
    for (unsigned int sample = 0u;
         sample < CURRENT_SENSOR_ZERO_SAMPLES; ++sample) {
        sum += read_sensor();
        sleep_us(100u);
    }
    const uint16_t zero = (uint16_t)(
        (sum + CURRENT_SENSOR_ZERO_SAMPLES / 2u) /
        CURRENT_SENSOR_ZERO_SAMPLES);
    monitor.raw = zero;
    if (!raw_is_present(zero)) {
        monitor.published.present = false;
        monitor.published.raw = zero;
        monitor.published.zero = monitor.zero;
        return false;
    }
    monitor.zero = zero;
    monitor.sum = 0;
    monitor.absolute_sum = 0;
    monitor.samples = 0u;
    monitor.published.present = true;
    monitor.published.raw = zero;
    monitor.published.zero = zero;
    monitor.window_started_ms = to_ms_since_boot(get_absolute_time());
    return true;
}

void current_monitor_get_snapshot(current_monitor_snapshot_t *snapshot) {
    if (snapshot != NULL) {
        *snapshot = monitor.published;
    }
}

void current_monitor_report(void) {
    current_monitor_snapshot_t snapshot;
    current_monitor_get_snapshot(&snapshot);
    printf("[MSG:M982 source=PICO_ADC channel=X-A pin=GP%u adc=%u "
           "present=%u raw=%u zero=%u avg=%.4fA abs_avg=%.4fA "
           "min=%.4fA max=%.4fA sensitivity=%.3fV/A]\r\n",
           PIN_CURRENT_SENSOR_XA_GPIO, PIN_CURRENT_SENSOR_XA_ADC_INPUT,
           snapshot.present ? 1u : 0u, snapshot.raw, snapshot.zero,
           (double)snapshot.average_a, (double)snapshot.absolute_average_a,
           (double)snapshot.minimum_a, (double)snapshot.maximum_a,
           (double)CURRENT_SENSOR_SENSITIVITY_V_PER_A);
}
