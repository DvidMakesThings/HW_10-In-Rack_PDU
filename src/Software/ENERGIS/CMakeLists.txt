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

project(Test C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ENERGIS 
    ENERGIS_core0_task.c
    drivers/CAT24C512_driver.c
    drivers/ILI9488_driver.c
    drivers/MCP23017_dual_driver.c
    drivers/MCP23017_display_driver.c
    drivers/MCP23017_relay_driver.c
    drivers/MCP23017_selection_driver.c
    drivers/button_driver.c
    drivers/HLW8032_driver.c
    misc/uart_command_handler.c
    network/timer.c
    network/w5x00_gpio_irq.c
    network/w5x00_spi.c
    network/wizchip_conf.c
    network/w5500.c
    network/socket.c
    network/snmp.c
    network/snmp_custom.c
    web_handlers/http_server.c
    web_handlers/control_handler.c
    web_handlers/settings_handler.c
    web_handlers/form_helpers.c
    web_handlers/status_handler.c
    html/web_pages.c
    utils/EEPROM_MemoryMap.c
    utils/helper_functions.c
    PDU_display.c
    ENERGIS_startup.c
    ENERGIS_core1_task.c
)


pico_set_program_name(ENERGIS "ENERGIS")
pico_set_program_version(ENERGIS "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_usb(ENERGIS 1)
pico_enable_stdio_uart(ENERGIS 1)

# Enable float support in printf
target_compile_definitions(ENERGIS PRIVATE
    PICO_STDIO_ENABLE_FLOAT=1
    PICO_PRINTF_ALWAYS_INCLUDE_FLOAT=1
)

# Add the standard library and necessary Pico libraries to the build
target_link_libraries(ENERGIS 
    pico_stdlib         # Standard Pico SDK
    pico_multicore      # Multicore support
    hardware_vreg       # VREG and chip reset
    hardware_spi        # SPI library
    hardware_i2c        # I2C library
    hardware_gpio       # GPIO control
    hardware_pwm        # PWM control (for backlight)
    hardware_sync       # Synchronization
    hardware_watchdog   # Watchdog timer
    hardware_clocks     # Clock configuration
    hardware_resets     # Reset controller
    hardware_dma        # DMA controller
    hardware_adc        # ADC controller
)

# Add the standard include files to the build.
target_include_directories(ENERGIS PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/html
    ${CMAKE_CURRENT_SOURCE_DIR}/misc
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/web_handlers
    ${PICO_SDK_PATH}/src/rp2_common
    
)

pico_add_extra_outputs(ENERGIS)
