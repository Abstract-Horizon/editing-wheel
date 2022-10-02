
# /home/pi/pico/pico-sdk/src/rp2_common/pico_stdio_usb/stdio_usb.c

set(PROJECT cdc_plus_hid)

cmake_minimum_required(VERSION 3.12)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(${PROJECT} C CXX ASM)

pico_sdk_init()

add_executable(${PROJECT} main.c)

target_sources(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/msc_disk.c
    ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/core1_loop.c
    ${CMAKE_CURRENT_LIST_DIR}/pid.c
    ${CMAKE_CURRENT_LIST_DIR}/nxjson.c
)

target_include_directories(${PROJECT} PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(${PROJECT}
    pico_stdlib
    tinyusb_device
    tinyusb_board
    hardware_i2c
    hardware_pwm
    pico_multicore
)

pico_add_extra_outputs(${PROJECT})

pico_enable_stdio_usb(${PROJECT} 1)
pico_enable_stdio_uart(${PROJECT} 0)
