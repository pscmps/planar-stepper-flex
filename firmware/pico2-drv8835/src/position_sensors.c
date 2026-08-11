#include "position_sensors.h"

#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "config.h"
#include "position_sensor_math.h"

enum {
    JOINT_COUNT = 2,
};

typedef struct {
    i2c_inst_t *bus;
    unsigned int sda_pin;
    unsigned int scl_pin;
    position_sensor_snapshot_t snapshot;
} sensor_state_t;

static sensor_state_t sensors[JOINT_COUNT] = {
    {.bus = i2c0, .sda_pin = PIN_AS5600_J1_SDA, .scl_pin = PIN_AS5600_J1_SCL},
    {.bus = i2c1, .sda_pin = PIN_AS5600_J2_SDA, .scl_pin = PIN_AS5600_J2_SCL},
};
static unsigned int next_joint;
static uint32_t next_poll_ms;
static bool status_enabled;

static bool read_sensor(sensor_state_t *sensor) {
    uint8_t register_address = 0x0bu;
    uint8_t response[3] = {0u, 0u, 0u};
    const int written = i2c_write_timeout_us(
        sensor->bus, AS5600_I2C_ADDRESS, &register_address, 1u, true,
        AS5600_I2C_TIMEOUT_US);
    if (written != 1) {
        sensor->snapshot.present = false;
        sensor->snapshot.magnet_detected = false;
        return false;
    }
    const int received = i2c_read_timeout_us(
        sensor->bus, AS5600_I2C_ADDRESS, response, sizeof(response), false,
        AS5600_I2C_TIMEOUT_US);
    if (received != (int)sizeof(response)) {
        sensor->snapshot.present = false;
        sensor->snapshot.magnet_detected = false;
        return false;
    }
    sensor->snapshot.status = response[0];
    sensor->snapshot.raw =
        (uint16_t)(((uint16_t)(response[1] & 0x0fu) << 8u) | response[2]);
    sensor->snapshot.degrees = as5600_raw_degrees(sensor->snapshot.raw);
    sensor->snapshot.magnet_detected = (sensor->snapshot.status & 0x20u) != 0u;
    sensor->snapshot.present = true;
    sensor->snapshot.updated_ms = to_ms_since_boot(get_absolute_time());
    return true;
}

static void configure_bus(sensor_state_t *sensor) {
    i2c_init(sensor->bus, AS5600_I2C_FREQUENCY_HZ);
    gpio_set_function(sensor->sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(sensor->scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sensor->sda_pin);
    gpio_pull_up(sensor->scl_pin);
}

void position_sensors_init(void) {
    memset(&sensors[0].snapshot, 0, sizeof(sensors[0].snapshot));
    memset(&sensors[1].snapshot, 0, sizeof(sensors[1].snapshot));
    configure_bus(&sensors[0]);
    configure_bus(&sensors[1]);
    (void)read_sensor(&sensors[0]);
    (void)read_sensor(&sensors[1]);
    next_joint = 0u;
    next_poll_ms = to_ms_since_boot(get_absolute_time()) +
                   AS5600_POLL_INTERVAL_MS;
    status_enabled = false;
}

void position_sensors_refresh(void) {
    (void)read_sensor(&sensors[0]);
    (void)read_sensor(&sensors[1]);
    next_poll_ms = to_ms_since_boot(get_absolute_time()) +
                   AS5600_POLL_INTERVAL_MS;
}

void position_sensors_service(void) {
    if (!status_enabled) {
        return;
    }
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if ((int32_t)(now_ms - next_poll_ms) < 0) {
        return;
    }
    (void)read_sensor(&sensors[next_joint]);
    next_joint = (next_joint + 1u) % JOINT_COUNT;
    next_poll_ms = now_ms + AS5600_POLL_INTERVAL_MS;
}

void position_sensors_set_status_enabled(bool enabled) {
    status_enabled = enabled;
}

bool position_sensors_status_enabled(void) {
    return status_enabled;
}

void position_sensors_get_snapshot(unsigned int joint,
                                   position_sensor_snapshot_t *snapshot) {
    if (snapshot == NULL) {
        return;
    }
    if (joint >= JOINT_COUNT) {
        memset(snapshot, 0, sizeof(*snapshot));
        return;
    }
    *snapshot = sensors[joint].snapshot;
}

void position_sensors_report(void) {
    position_sensor_snapshot_t joint1;
    position_sensor_snapshot_t joint2;
    position_sensors_get_snapshot(0u, &joint1);
    position_sensors_get_snapshot(1u, &joint2);
    printf("[MSG:M983 enabled=%u J1_bus=I2C0 J1_sda=GP%u J1_scl=GP%u "
           "J1_present=%u J1_magnet=%u J1_status=0x%02X J1_raw=%u "
           "J1_deg=%.3f J2_bus=I2C1 J2_sda=GP%u J2_scl=GP%u "
           "J2_present=%u J2_magnet=%u J2_status=0x%02X J2_raw=%u "
           "J2_deg=%.3f]\r\n",
           status_enabled ? 1u : 0u,
           PIN_AS5600_J1_SDA, PIN_AS5600_J1_SCL,
           joint1.present ? 1u : 0u, joint1.magnet_detected ? 1u : 0u,
           joint1.status, joint1.raw, (double)joint1.degrees,
           PIN_AS5600_J2_SDA, PIN_AS5600_J2_SCL,
           joint2.present ? 1u : 0u, joint2.magnet_detected ? 1u : 0u,
           joint2.status, joint2.raw, (double)joint2.degrees);
}
