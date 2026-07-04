# Generated CMake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
# Use -DPICO_SDK_PATH="C:/Users/sdvid/.pico-sdk/sdk/2.2.0"
if(DEFINED PICO_SDK_PATH)
  include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
elseif(DEFINED ENV{PICO_SDK_PATH})
  include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
else()
  message(FATAL_ERROR "PICO_SDK_PATH not set")
endif()

project(ENERGIS_RTOS C CXX ASM)
pico_sdk_init()

message(STATUS "PICO_SDK_PATH = ${PICO_SDK_PATH}")

# ---------------------------- FreeRTOS integration -------------------------
# 1) Tell the Kernel which port and heap to use for RP2040
set(FREERTOS_PORT "GCC_RP2040" CACHE STRING "")
set(FREERTOS_HEAP "4" CACHE STRING "")  # heap_4

# 2) Provide a target that points to the folder containing FreeRTOSConfig.h
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src
)
target_compile_definitions(freertos_config INTERFACE projCOVERAGE_TEST=0)

# 3) Add the FreeRTOS kernel (it will create freertos_kernel target)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel ${CMAKE_BINARY_DIR}/freertos_kernel)

# ----------------------------- Executable ----------------------------------
add_executable(ENERGIS_RTOS
    src/ENERGIS_RTOS.c
    src/drivers/button_driver.c
    src/drivers/i2c_bus.c
    src/drivers/MCP23017_driver.c
    src/drivers/CAT24C256_driver.c
    src/drivers/ethernet_driver.c
    src/drivers/snmp.c
    src/drivers/socket.c
    src/drivers/HLW8032_driver.c
    src/misc/helpers.c
    src/misc/rtos_hooks.c
    src/misc/crashlog.c
    src/misc/power_mgr.c
    src/snmp/snmp_custom.c
    src/snmp/snmp_networkCtrl.c
    src/snmp/snmp_outletCtrl.c
    src/snmp/snmp_powerMon.c
    src/snmp/snmp_voltageMon.c
    src/tasks/OCP.c
    src/tasks/InitTask.c
    src/tasks/LoggerTask.c
    src/tasks/ConsoleTask.c
    src/tasks/StorageTask.c
    src/tasks/NetTask.c
    src/tasks/MeterTask.c
    src/tasks/ButtonTask.c
    src/tasks/HealthTask.c
    src/tasks/SwitchTask.c
    src/tasks/provisioning_commands.c
    src/tasks/storage_submodule/calibration.c
    src/tasks/storage_submodule/channel_labels.c
    src/tasks/storage_submodule/energy_monitor.c
    src/tasks/storage_submodule/event_log.c
    src/tasks/storage_submodule/factory_defaults.c
    src/tasks/storage_submodule/network.c
    src/tasks/storage_submodule/storage_common.c
    src/tasks/storage_submodule/user_output.c
    src/tasks/storage_submodule/user_prefs.c
    src/tasks/storage_submodule/device_identity.c
    src/tasks/storage_submodule/auth_store.c
    src/web_handlers/http_server.c
    src/web_handlers/control_handler.c
    src/web_handlers/settings_handler.c
    src/web_handlers/status_handler.c
    src/web_handlers/page_content.c
    src/web_handlers/metrics_handler.c
    src/web_handlers/preset_handler.c
    src/web_handlers/auth.c
    src/wrap_watchdog.c
)

target_include_directories(ENERGIS_RTOS PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(ENERGIS_RTOS
    pico_stdlib
    hardware_gpio
    freertos_kernel
    freertos_config          
    hardware_vreg       
    hardware_spi        
    hardware_i2c        
    hardware_gpio       
    hardware_pwm        
    hardware_sync       
    hardware_watchdog
    hardware_clocks
    hardware_resets
    hardware_dma
    hardware_adc
)

target_link_options(ENERGIS_RTOS PRIVATE
  -Wl,--wrap=watchdog_reboot
  -Wl,--wrap=abort
  -Wl,--wrap=__assert_func
  -Wl,--wrap=panic
  -Wl,--wrap=hard_assertion_failure
  -Wl,--wrap=invalid_params
  -Wl,--wrap=reset_usb_boot
  -Wl,--trace-symbol,watchdog_reboot
  -Wl,--trace-symbol,abort
  -Wl,--trace-symbol,__assert_func
  -Wl,--trace-symbol,panic
  -Wl,--trace-symbol,hard_assertion_failure
  -Wl,--trace-symbol,invalid_params
  -Wl,--trace-symbol,reset_usb_boot
  -Wl,--wrap=runtime_unreset_core
  -Wl,--wrap=runtime_reboot
  -Wl,--wrap=__NVIC_SystemReset
  -Wl,--wrap=scb_reboot
)

# ----------------------------- Pico settings -------------------------------
# Read firmware version and variant from CONFIG.h (single source of truth)
file(READ "${CMAKE_CURRENT_LIST_DIR}/src/CONFIG.h" _config_h_contents)
string(REGEX MATCH "#define FIRMWARE_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"" _ "${_config_h_contents}")
set(FIRMWARE_VERSION "${CMAKE_MATCH_1}")
if(NOT FIRMWARE_VERSION)
    message(FATAL_ERROR "Could not parse FIRMWARE_VERSION from src/CONFIG.h")
endif()

# Detect variant: only look at the user-selection lines before #if defined(...)
# Match lines like: #define ENERGIS_VARIANT_19IN  (not preceded by //)
# Exclude commented lines (// #define ...) and the fallback inside #else
string(REGEX MATCH "\n[ \t]*#define[ \t]+ENERGIS_VARIANT_([0-9]+IN)[ \t]*\n" _ "${_config_h_contents}")
if(CMAKE_MATCH_1)
    set(ENERGIS_VARIANT "${CMAKE_MATCH_1}")
else()
    set(ENERGIS_VARIANT "10IN")
endif()

# Map variant to suffix
if(ENERGIS_VARIANT STREQUAL "19IN")
    set(VARIANT_SUFFIX "variant-19")
elseif(ENERGIS_VARIANT STREQUAL "10IN")
    set(VARIANT_SUFFIX "variant-10")
else()
    set(VARIANT_SUFFIX "variant-10")
endif()

message(STATUS "Firmware version: ${FIRMWARE_VERSION}, Variant: ${ENERGIS_VARIANT}")
set(FIRMWARE_BASE_NAME "ENERGIS_firmware_v${FIRMWARE_VERSION}_${VARIANT_SUFFIX}")

# USB / internal descriptors
pico_set_program_name(ENERGIS_RTOS "${FIRMWARE_BASE_NAME}")
pico_set_program_version(ENERGIS_RTOS "${FIRMWARE_VERSION}")

# Change the primary target's output filename (affects ENERGIS_RTOS.elf -> ENERGIS_firmware_v1.0.0.elf)
set_target_properties(ENERGIS_RTOS PROPERTIES OUTPUT_NAME "${FIRMWARE_BASE_NAME}")

pico_enable_stdio_usb(ENERGIS_RTOS 1)
pico_enable_stdio_uart(ENERGIS_RTOS 0)

pico_add_extra_outputs(ENERGIS_RTOS)

# The Pico SDK's extra outputs (uf2/bin/hex) are still generated using the TARGET name in some versions
# To guarantee we have consistently renamed artifacts, copy them with the desired versioned base name.
add_custom_command(TARGET ENERGIS_RTOS POST_BUILD
    COMMENT "Ensuring versioned firmware artifacts present"
    COMMAND ${CMAKE_COMMAND} -E echo "Source UF2: ${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_BASE_NAME}.uf2"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_BASE_NAME}.uf2"
      "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_BASE_NAME}.uf2"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_BASE_NAME}.bin"
      "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_BASE_NAME}.bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_BASE_NAME}.hex"
      "${CMAKE_CURRENT_BINARY_DIR}/${FIRMWARE_BASE_NAME}.hex"
)
