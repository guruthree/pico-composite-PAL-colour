cmake_minimum_required(VERSION 3.13)

# these need to be early to be honored on the first cmake
set(PICO_BOARD "pimoroni_tiny2040" CACHE STRING "" FORCE)

include(pico_sdk_import.cmake)

project(pio-DAC_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-usage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-usage")

add_executable(pio-DAC
    pio-DAC.cpp
)

pico_generate_pio_header(pio-DAC ${CMAKE_CURRENT_LIST_DIR}/dac.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_include_directories(pio-DAC PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pio-DAC
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    pico_malloc
	pico_mem_ops
    pico_float
)
pico_add_extra_outputs(pio-DAC)

pico_enable_stdio_usb(pio-DAC 1)
pico_enable_stdio_uart(pio-DAC 0)

