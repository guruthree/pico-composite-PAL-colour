cmake_minimum_required(VERSION 3.13)

# these need to be early to be honored on the first cmake
#set(PICO_BOARD "pimoroni_tiny2040" CACHE STRING "" FORCE)
set(PICO_BOARD "pico" CACHE STRING "" FORCE)

#set(PICO_COPY_TO_RAM 1  CACHE STRING "" FORCE)

include(pico_sdk_import.cmake)

project(pico-composite-PAL-colour C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-usage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-usage")

add_executable(pico-composite-PAL-colour
    pico-composite-PAL-colour.cpp
)

pico_generate_pio_header(pico-composite-PAL-colour ${CMAKE_CURRENT_LIST_DIR}/dac.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

include_directories(includes includes/images demos)

target_include_directories(pico-composite-PAL-colour PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pico-composite-PAL-colour
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_dma
    pico_malloc
	pico_mem_ops
    pico_float
)
pico_add_extra_outputs(pico-composite-PAL-colour)

pico_enable_stdio_usb(pico-composite-PAL-colour 0)
pico_enable_stdio_uart(pico-composite-PAL-colour 0)

# for CPU clocks above 266 MHz, slow down access to the flash
pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
pico_set_boot_stage2(pico-composite-PAL-colour slower_boot2)
