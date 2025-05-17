# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ENERGIS C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(ENERGIS 
    ENERGIS.c
    drivers/CAT24C512_driver.c
    drivers/ILI9488_driver.c
    drivers/MCP23017_display_driver.c
    drivers/MCP23017_relay_driver.c
    drivers/button_driver.c
    misc/uart_command_handler.c
    network/timer.c
    network/w5x00_gpio_irq.c
    network/w5x00_spi.c
    network/wizchip_conf.c
    network/w5500.c
    network/socket.c
    network/snmp.c
    network/snmp_custom.c
    html/web_pages.c
    utils/EEPROM_MemoryMap.c
    utils/helper_functions.c
    PDU_display.c
    startup.c
    core0_task.c
    core1_snmp.c
)


pico_set_program_name(ENERGIS "ENERGIS")
pico_set_program_version(ENERGIS "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_usb(ENERGIS 1)
pico_enable_stdio_uart(ENERGIS 0)


# Add the standard library and necessary Pico libraries to the build
target_link_libraries(ENERGIS 
    pico_stdlib         # Standard Pico SDK
    pico_multicore      # Multicore support
    hardware_spi        # SPI library
    hardware_i2c        # I2C library
    hardware_gpio       # GPIO control
    hardware_pwm        # PWM control (for backlight)
    hardware_sync       # Synchronization
    hardware_watchdog   # Watchdog timer
    hardware_clocks     # Clock configuration
    hardware_resets     # Reset controller
    hardware_dma        # DMA controller
)

# Add the standard include files to the build.
target_include_directories(ENERGIS PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/network/port/ioLibrary_Driver/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/network/port/timer
    ${CMAKE_CURRENT_LIST_DIR}/FatFS
    ${PICO_SDK_PATH}/src/rp2_common/hardware_spi/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_i2c/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_pwm/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_resets/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_watchdog/include   # For watchdog.h
    ${PICO_SDK_PATH}/src/rp2_common/hardware_clocks/include     # For clocks.h
    ${PICO_SDK_PATH}/src/rp2_common/hardware_structs/include    # For scb.h and vreg_and_chip_reset.h
    ${PICO_SDK_PATH}/src/rp2_common/hardware_base/include       # Additional base hardware includes
    
)

pico_add_extra_outputs(ENERGIS)
