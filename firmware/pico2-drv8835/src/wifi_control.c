#include "wifi_control.h"

#include <stdio.h>
#include <string.h>

#include "lwip/apps/httpd.h"
#include "lwip/ip_addr.h"
#include "pico/critical_section.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"

#include "config.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "planner.h"
#include "stepper.h"

#define WIFI_PASSWORD "planar-stepper"

static critical_section_t control_lock;
static bool lock_ready = false;
static bool network_ready = false;
static bool request_pending = false;
static bool mode_pending = false;
static bool pen_pending = false;
static bool requested_pen_up = true;
static wifi_drive_request_t requested_drive = {
    .microsteps = DRV8835_DEFAULT_MICROSTEPS,
    .capture_ms = DRV8835_DEFAULT_CAPTURE_MS,
    .active_axes_only = DRV8835_DEFAULT_ACTIVE_AXES_ONLY != 0,
};
static bool request_busy = false;
static bool stop_pending = false;
static int cached_ap_clients = 0;
static wifi_jog_request_t request_slot;
static wifi_jog_status_t status_snapshot;
static char last_result[16] = "starting";
static char ap_ssid[32] = "PlanarStepper";
static dhcp_server_t dhcp_server;
static dns_server_t dns_server;

static void set_result_locked(const char *result) {
    snprintf(last_result, sizeof(last_result), "%s", result ? result : "unknown");
    status_snapshot.last_result = last_result;
}

static const char *jog_cgi(int index, int count, char *names[], char *values[]) {
    (void)index;
    wifi_jog_request_t request;
    if (!wifi_jog_parse_params(count, names, values, &request)) {
        critical_section_enter_blocking(&control_lock);
        set_result_locked("invalid");
        critical_section_exit(&control_lock);
        return "/api/400.json";
    }

    critical_section_enter_blocking(&control_lock);
    const bool accepted = !request_busy &&
        wifi_jog_queue_offer(&request_slot, &request_pending, &request);
    if (accepted) {
        request_busy = true;
        set_result_locked("queued");
    } else {
        set_result_locked("busy");
    }
    critical_section_exit(&control_lock);
    return accepted ? "/api/accepted.json" : "/api/busy.json";
}

static const char *stop_cgi(int index, int count, char *names[], char *values[]) {
    (void)index;
    (void)count;
    (void)names;
    (void)values;
    critical_section_enter_blocking(&control_lock);
    request_pending = false;
    mode_pending = false;
    pen_pending = false;
    stop_pending = true;
    request_busy = true;
    set_result_locked("stopping");
    critical_section_exit(&control_lock);
    planner_request_cancel();
    return "/api/stopped.json";
}

static const char *pen_cgi(int index, int count, char *names[], char *values[]) {
    (void)index;
    bool pen_up = false;
    if (!wifi_pen_parse_params(count, names, values, &pen_up)) {
        return "/api/400.json";
    }
    critical_section_enter_blocking(&control_lock);
    const bool accepted =
        !request_busy && !request_pending && !mode_pending && !pen_pending;
    if (accepted) {
        requested_pen_up = pen_up;
        pen_pending = true;
        request_busy = true;
        set_result_locked(pen_up ? "raising" : "lowering");
    } else {
        set_result_locked("busy");
    }
    critical_section_exit(&control_lock);
    return accepted ? "/api/accepted.json" : "/api/busy.json";
}

static const char *mode_cgi(int index, int count, char *names[], char *values[]) {
    (void)index;
    wifi_drive_request_t request;
    if (!wifi_mode_parse_params(count, names, values, &request)) {
        return "/api/400.json";
    }
    critical_section_enter_blocking(&control_lock);
    const bool accepted =
        !request_busy && !request_pending && !mode_pending && !pen_pending;
    if (accepted) {
        requested_drive = request;
        mode_pending = true;
        request_busy = true;
        set_result_locked("configuring");
    } else {
        set_result_locked("busy");
    }
    critical_section_exit(&control_lock);
    return accepted ? "/api/accepted.json" : "/api/busy.json";
}

static const char *captive_cgi(int index, int count, char *names[], char *values[]) {
    (void)index;
    (void)count;
    (void)names;
    (void)values;
    return "/index.html";
}

static u16_t status_ssi(int index, char *insert, int insert_length) {
    (void)index;
    wifi_jog_status_t status;
    char result_copy[sizeof(last_result)];
    critical_section_enter_blocking(&control_lock);
    status = status_snapshot;
    snprintf(result_copy, sizeof(result_copy), "%s", last_result);
    status.last_result = result_copy;
    critical_section_exit(&control_lock);

    const int written = wifi_jog_format_status(&status, insert, (size_t)insert_length);
    if (written < 0) {
        return 0;
    }
    return (u16_t)(written < insert_length ? written : insert_length - 1);
}

bool wifi_control_init(void) {
    if (!lock_ready) {
        critical_section_init(&control_lock);
        lock_ready = true;
    }
    status_snapshot.last_result = last_result;

    if (cyw43_arch_init() != 0) {
        set_result_locked("wifi_error");
        return false;
    }

    char unique_id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(unique_id, sizeof(unique_id));
    wifi_jog_make_ssid(unique_id, ap_ssid, sizeof(ap_ssid));
    cyw43_arch_enable_ap_mode(ap_ssid, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

    ip_addr_t gateway;
    ip_addr_t mask;
    IP4_ADDR(ip_2_ip4(&gateway), 192, 168, 4, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    static const tCGI handlers[] = {
        {"/api/jog.cgi", jog_cgi},
        {"/api/stop.cgi", stop_cgi},
        {"/api/mode.cgi", mode_cgi},
        {"/api/pen.cgi", pen_cgi},
        {"/generate_204", captive_cgi},
        {"/hotspot-detect.html", captive_cgi},
        {"/connecttest.txt", captive_cgi},
        {"/ncsi.txt", captive_cgi},
    };
    static const char *ssi_tags[] = {"status"};
    cyw43_arch_lwip_begin();
    const int dhcp_result = dhcp_server_init(&dhcp_server, &gateway, &mask);
    dns_server_init(&dns_server, &gateway);
    http_set_cgi_handlers(handlers, sizeof(handlers) / sizeof(handlers[0]));
    http_set_ssi_handler(status_ssi, ssi_tags, sizeof(ssi_tags) / sizeof(ssi_tags[0]));
    httpd_init();
    cyw43_arch_lwip_end();

    if (dhcp_result != 0) {
        printf("[WIFI ERROR: DHCP server init failed result=%d]\r\n", dhcp_result);
    }

    critical_section_enter_blocking(&control_lock);
    network_ready = true;
    status_snapshot.ready = true;
    set_result_locked("ready");
    critical_section_exit(&control_lock);
    return true;
}

bool wifi_control_take_jog(wifi_jog_request_t *request) {
    critical_section_enter_blocking(&control_lock);
    const bool available = wifi_jog_queue_take(&request_slot, &request_pending, request);
    critical_section_exit(&control_lock);
    return available;
}

bool wifi_control_take_mode(wifi_drive_request_t *request) {
    if (!request) {
        return false;
    }
    critical_section_enter_blocking(&control_lock);
    const bool available = mode_pending;
    if (available) {
        *request = requested_drive;
        mode_pending = false;
    }
    critical_section_exit(&control_lock);
    return available;
}

bool wifi_control_take_pen(bool *pen_up) {
    if (!pen_up) {
        return false;
    }
    critical_section_enter_blocking(&control_lock);
    const bool available = pen_pending;
    if (available) {
        *pen_up = requested_pen_up;
        pen_pending = false;
    }
    critical_section_exit(&control_lock);
    return available;
}

bool wifi_control_take_stop(void) {
    critical_section_enter_blocking(&control_lock);
    const bool requested = stop_pending;
    stop_pending = false;
    critical_section_exit(&control_lock);
    return requested;
}

void wifi_control_set_running(void) {
    critical_section_enter_blocking(&control_lock);
    request_busy = true;
    set_result_locked("running");
    critical_section_exit(&control_lock);
}

void wifi_control_set_result(const char *result) {
    critical_section_enter_blocking(&control_lock);
    request_busy = false;
    set_result_locked(result);
    critical_section_exit(&control_lock);
}

void wifi_control_update_status(const controller_state_t *state) {
    if (!state || !lock_ready) {
        return;
    }
    bool active_axes_only = false;
    unsigned int microsteps = 0u;
    unsigned int capture_ms = 0u;
    unsigned int active_mask = 0u;
    int x_angle = 0;
    int y_angle = 0;
    const char *x_phase = NULL;
    const char *y_phase = NULL;
    stepper_drv8835_get_config(&microsteps, NULL, NULL, NULL, NULL, NULL,
                               NULL, NULL, &capture_ms, &active_axes_only);
    stepper_drv8835_get_motion_status(&active_mask, NULL, NULL, NULL,
                                      &x_angle, &y_angle, &x_phase, &y_phase);
    critical_section_enter_blocking(&control_lock);
    status_snapshot.ready = network_ready && !request_busy;
    status_snapshot.busy = request_busy;
    status_snapshot.armed = state->enabled;
    status_snapshot.outputs = stepper_drv8835_outputs_active();
    status_snapshot.active_axes_only = active_axes_only;
    status_snapshot.microsteps = microsteps;
    status_snapshot.capture_ms = capture_ms;
    status_snapshot.active_mask = active_mask;
    status_snapshot.x_angle = x_angle;
    status_snapshot.y_angle = y_angle;
    status_snapshot.x_phase = x_phase;
    status_snapshot.y_phase = y_phase;
    status_snapshot.clients = cached_ap_clients > 0 ? (unsigned int)cached_ap_clients : 0u;
    status_snapshot.x = state->work_x;
    status_snapshot.y = state->work_y;
    status_snapshot.z = state->work_z;
    status_snapshot.pen_pulse_us =
        state->pen_pwm > 0 ? (unsigned int)state->pen_pwm : 0u;
    status_snapshot.pen_up = state->pen_up;
    status_snapshot.last_result = last_result;
    critical_section_exit(&control_lock);
}

const char *wifi_control_ssid(void) {
    return ap_ssid;
}

int wifi_control_ap_client_count(void) {
    if (!network_ready) {
        return 0;
    }
    int max_stas = 0;
    cyw43_wifi_ap_get_max_stas(&cyw43_state, &max_stas);
    if (max_stas <= 0) {
        return -1;
    }
    enum { MAC_BYTES = 6, LOCAL_MAX_STAS = 8 };
    if (max_stas > LOCAL_MAX_STAS) {
        max_stas = LOCAL_MAX_STAS;
    }
    uint8_t macs[LOCAL_MAX_STAS * MAC_BYTES];
    int count = max_stas;
    cyw43_wifi_ap_get_stas(&cyw43_state, &count, macs);
    cached_ap_clients = count;
    return count;
}
