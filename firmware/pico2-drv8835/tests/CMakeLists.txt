cmake_minimum_required(VERSION 3.20)
project(pico2_drv8835_host_tests C)

set(CMAKE_C_STANDARD 11)
enable_testing()

add_executable(drv8835_motion_math_tests
    test_drv8835_motion_math.c
    ../src/drv8835_motion_math.c
)
target_include_directories(drv8835_motion_math_tests PRIVATE ../include)
target_compile_options(drv8835_motion_math_tests PRIVATE -Wall -Wextra -Wpedantic)
if(NOT MSVC)
    target_link_libraries(drv8835_motion_math_tests m)
endif()
add_test(NAME drv8835_motion_math_tests COMMAND drv8835_motion_math_tests)

add_executable(wifi_jog_protocol_tests
    test_wifi_jog_protocol.c
    ../src/wifi_jog_protocol.c
)
target_include_directories(wifi_jog_protocol_tests PRIVATE ../include)
target_compile_options(wifi_jog_protocol_tests PRIVATE -Wall -Wextra -Wpedantic)
if(NOT MSVC)
    target_link_libraries(wifi_jog_protocol_tests m)
endif()
add_test(NAME wifi_jog_protocol_tests COMMAND wifi_jog_protocol_tests)

add_executable(servo_pen_math_tests
    test_servo_pen_math.c
    ../src/servo_pen_math.c
)
target_include_directories(servo_pen_math_tests PRIVATE ../include)
target_compile_options(servo_pen_math_tests PRIVATE -Wall -Wextra -Wpedantic)
if(NOT MSVC)
    target_link_libraries(servo_pen_math_tests m)
endif()
add_test(NAME servo_pen_math_tests COMMAND servo_pen_math_tests)

add_executable(current_monitor_math_tests
    test_current_monitor_math.c
)
target_include_directories(current_monitor_math_tests PRIVATE ../include)
target_compile_options(current_monitor_math_tests PRIVATE -Wall -Wextra -Wpedantic)
if(NOT MSVC)
    target_link_libraries(current_monitor_math_tests m)
endif()
add_test(NAME current_monitor_math_tests COMMAND current_monitor_math_tests)

add_executable(position_sensor_math_tests
    test_position_sensor_math.c
)
target_include_directories(position_sensor_math_tests PRIVATE ../include)
target_compile_options(position_sensor_math_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME position_sensor_math_tests COMMAND position_sensor_math_tests)
