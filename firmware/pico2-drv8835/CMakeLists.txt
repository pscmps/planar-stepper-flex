cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD pico2 CACHE STRING "Target board")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Pico SDK platform")

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico2_drv8835_planar_stepper C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(DRV8835_SOURCES
    src/main.c
    src/gcode.c
    src/planner.c
    src/stepper_drv8835.c
    src/drv8835_motion_math.c
    src/servo_pen.c
    src/servo_pen_math.c
    src/current_monitor.c
    src/position_sensors.c
)

add_executable(pico2_gcode_drv8835_xy ${DRV8835_SOURCES})
target_include_directories(pico2_gcode_drv8835_xy PRIVATE include)
target_compile_definitions(pico2_gcode_drv8835_xy PRIVATE MOTION_BACKEND_DRV8835=1)
target_compile_options(pico2_gcode_drv8835_xy PRIVATE -Wall -Wextra)
target_link_libraries(pico2_gcode_drv8835_xy
    pico_stdlib hardware_clocks hardware_gpio hardware_pwm hardware_adc
    hardware_i2c hardware_timer
)
pico_enable_stdio_usb(pico2_gcode_drv8835_xy 1)
pico_enable_stdio_uart(pico2_gcode_drv8835_xy 0)
pico_add_extra_outputs(pico2_gcode_drv8835_xy)

add_executable(pico2_gcode_drv8835_xy_test src/drv8835_y_test.c)
target_include_directories(pico2_gcode_drv8835_xy_test PRIVATE include)
target_compile_definitions(pico2_gcode_drv8835_xy_test PRIVATE MOTION_BACKEND_DRV8835=1)
target_compile_options(pico2_gcode_drv8835_xy_test PRIVATE -Wall -Wextra)
target_link_libraries(pico2_gcode_drv8835_xy_test
    pico_stdlib hardware_clocks hardware_gpio hardware_pwm hardware_timer
)
pico_enable_stdio_usb(pico2_gcode_drv8835_xy_test 1)
pico_enable_stdio_uart(pico2_gcode_drv8835_xy_test 0)
pico_add_extra_outputs(pico2_gcode_drv8835_xy_test)

if(PICO_CYW43_SUPPORTED)
    add_executable(pico2w_gcode_drv8835_wifi
        src/main_wifi.c
        src/gcode.c
        src/planner.c
        src/stepper_drv8835.c
        src/drv8835_motion_math.c
        src/servo_pen.c
        src/servo_pen_math.c
        src/current_monitor.c
        src/position_sensors.c
        src/wifi_control.c
        src/wifi_jog_protocol.c
        third_party/pico_examples/dhcpserver.c
        third_party/pico_examples/dnsserver.c
    )
    target_include_directories(pico2w_gcode_drv8835_wifi PRIVATE
        include third_party/pico_examples
    )
    target_compile_definitions(pico2w_gcode_drv8835_wifi PRIVATE
        MOTION_BACKEND_DRV8835=1 MOTION_WIFI_AP=1
    )
    target_compile_options(pico2w_gcode_drv8835_wifi PRIVATE -Wall -Wextra)
    target_link_libraries(pico2w_gcode_drv8835_wifi
        pico_stdlib pico_cyw43_arch_lwip_threadsafe_background pico_lwip_http
        pico_unique_id planar_wifi_http_content hardware_clocks hardware_gpio
        hardware_pwm hardware_adc hardware_i2c hardware_timer
    )
    pico_configure_ip4_address(pico2w_gcode_drv8835_wifi PRIVATE
        CYW43_DEFAULT_IP_AP_ADDRESS "192.168.4.1")
    pico_enable_stdio_usb(pico2w_gcode_drv8835_wifi 1)
    pico_enable_stdio_uart(pico2w_gcode_drv8835_wifi 0)
    pico_add_extra_outputs(pico2w_gcode_drv8835_wifi)

    pico_add_library(planar_wifi_http_content NOFLAG)
    pico_set_lwip_httpd_content(planar_wifi_http_content INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/web/404.html
        ${CMAKE_CURRENT_LIST_DIR}/web/index.html
        ${CMAKE_CURRENT_LIST_DIR}/web/style.css
        ${CMAKE_CURRENT_LIST_DIR}/web/app.js
        ${CMAKE_CURRENT_LIST_DIR}/web/api/status.shtml
        ${CMAKE_CURRENT_LIST_DIR}/web/api/accepted.json
        ${CMAKE_CURRENT_LIST_DIR}/web/api/stopped.json
        ${CMAKE_CURRENT_LIST_DIR}/web/api/busy.json
        ${CMAKE_CURRENT_LIST_DIR}/web/api/400.json
    )
else()
    message(STATUS "Pico W support unavailable for ${PICO_BOARD}; Wi-Fi target skipped")
endif()
