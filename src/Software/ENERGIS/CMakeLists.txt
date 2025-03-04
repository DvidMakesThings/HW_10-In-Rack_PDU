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
    drivers/MCP23017_driver.c
    drivers/w5500_driver.c
    PDU_display.c
)

pico_set_program_name(ENERGIS "ENERGIS")
pico_set_program_version(ENERGIS "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(ENERGIS 1)
pico_enable_stdio_usb(ENERGIS 1)

# Add the standard library and necessary Pico libraries to the build
target_link_libraries(ENERGIS pico_stdlib pico_multicore hardware_spi hardware_i2c)

# Add the standard include files to the build.
# The FatFS directory is added here, as well as the directories for SPI and I2C.
target_include_directories(ENERGIS PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/FatFS
    ${PICO_SDK_PATH}/src/rp2_common/hardware_spi/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_i2c/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_pwm/include
)

pico_add_extra_outputs(ENERGIS)
