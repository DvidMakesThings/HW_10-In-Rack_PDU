/**
 * @file src/CONFIG.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup config Configuration Files
 * @brief Configuration files for the Energis PDU firmware.
 * @{
 *
 * @defgroup config01 1. RTOS Configuration
 * @ingroup config
 * @brief Configuration header for ENERGIS PDU firmware.
 * @{
 *
 * @version 2.0.0
 * @date 2025-11-08
 *
 * @details This file contains global configuration settings, 
 * peripheral assignments, and logging macros for the ENERGIS PDU 
 * firmware project. It contains user-configurable options as well 
 * as system constants. It describes GPIO pin assignments, I2C/SPI 
 * peripheral usage, ADC channel assignments, and other peripheral 
 * configurations.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

/**
 * @file CONFIG.h
 * @author DvidMakesThings - David Sipos
 * @brief 
 * @version 1.0.0
 * @date 2025-11-06
 * @details
 * This file contains global configuration settings, peripheral assignments,
 * and logging macros for the ENERGIS PDU firmware project.
 * It contains user-configurable options as well as system constants.
 * It describes GPIO pin assignments, I2C/SPI peripheral usage,
 * ADC channel assignments, and other peripheral configurations.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "FreeRTOS.h"
#include "event_groups.h"
#include "hardware/irq.h"
#include "hardware/structs/vreg_and_chip_reset.h"
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "hardware/watchdog.h"

/* clang-format off */

#include "misc/EEPROM_MemoryMap.h"
#include "misc/helpers.h"
#include "misc/crashlog.h"
#include "misc/rtos_hooks.h"

#include "drivers/CAT24C512_driver.h"
#include "drivers/MCP23017_driver.h"
#include "drivers/button_driver.h"
#include "drivers/HLW8032_driver.h"
#include "drivers/ethernet_config.h"
#include "drivers/ethernet_driver.h"
#include "drivers/ethernet_w5500regs.h"
#include "drivers/snmp.h"
#include "drivers/socket.h"

#include "snmp/snmp_custom.h"
#include "snmp/snmp_networkCtrl.h"
#include "snmp/snmp_outletCtrl.h"
#include "snmp/snmp_powerMon.h"
#include "snmp/snmp_voltageMon.h"

#include "tasks/ConsoleTask.h"
#include "tasks/ButtonTask.h"
#include "tasks/LoggerTask.h"
#include "tasks/NetTask.h"
#include "tasks/StorageTask.h"
#include "tasks/MeterTask.h"
#include "tasks/InitTask.h"
#include "tasks/HealthTask.h"

#include "web_handlers/control_handler.h"
#include "web_handlers/http_server.h"
#include "web_handlers/page_content.h"
#include "web_handlers/settings_handler.h"
#include "web_handlers/status_handler.h"
/* clang-format on */

#include "serial_number.h"

extern w5500_NetConfig eth_netcfg;

/********************************************************************************
 *                          GLOBAL CONFIGURATIONS                               *
 *                          CONFIGURABLE BY USER                                *
 ********************************************************************************/

/********************** Button behavior for press durations *********************/
// Button longpress duration thresholds
#define LONGPRESS_DT 2500

// Debounce time for button presses
#define DEBOUNCE_MS 100u

#define POST_GUARD_MS (DEBOUNCE_MS + 10u)
#define BTN_LOG_ENABLE 1

/********************************************************************************
 *                          GLOBAL CONFIGURATIONS                               *
 *                       DO NOT TOUCH PART FROM HERE                            *
 ********************************************************************************/

/* ---------- Default values  ---------- */
#define DEFAULT_SN SERIAL_NUMBER
#define SWVERSION FIRMWARE_VERSION
#define SW_REV FIRMWARE_VERSION_LITERAL
#define DEFAULT_NAME "ENERGIS-" FIRMWARE_VERSION
#define DEFAULT_LOCATION "Location"

/* ---------- SYSTEM CONSTANTS ---------- */
#define LOGGER_STACK_SIZE 1024
#define LOGGER_QUEUE_LEN 64
#define LOGGER_MSG_MAX 128

/* ---------- LOGGING FLAGS ---------- */
#ifndef DEBUG
#define DEBUG 0
#endif
#ifndef DEBUG_HEALTH
#define DEBUG_HEALTH 0
#endif
#ifndef INFO
#define INFO 1
#endif
#ifndef ERROR
#define ERROR 1
#endif
#ifndef WARNING
#define WARNING 1
#endif
#ifndef PLOT_EN
#define PLOT_EN 0
#endif
#ifndef NETLOG
#define NETLOG 0
#endif
#ifndef UART_IFACE
#define UART_IFACE 1
#endif

#if DEBUG
#define DEBUG_PRINT(...) log_printf("\t[DEBUG] " __VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)0)
#endif

#if DEBUG_HEALTH
#define DEBUG_PRINT_HEALTH(...) log_printf("\t[HEALTH] " __VA_ARGS__)
#else
#define DEBUG_PRINT_HEALTH(...) ((void)0)
#endif

#if INFO
#define INFO_PRINT(...) log_printf("[INFO] " __VA_ARGS__)
#else
#define INFO_PRINT(...) ((void)0)
#endif

#if ERROR
#define ERROR_PRINT(...) log_printf("[ERROR] " __VA_ARGS__)
#else
#define ERROR_PRINT(...) ((void)0)
#endif

#if WARNING
#define WARNING_PRINT(...) log_printf("[WARNING] " __VA_ARGS__)
#else
#define WARNING_PRINT(...) ((void)0)
#endif

#if PLOT_EN
#define PLOT(...) log_printf("\t[PLOT] " __VA_ARGS__)
#else
#define PLOT(...) ((void)0)
#endif

#if NETLOG
#define NETLOG_PRINT(...) log_printf("[NETLOG] " __VA_ARGS__)
#else
#define NETLOG_PRINT(...) ((void)0)
#endif

#if UART_IFACE
#define ECHO(...) log_printf("\t[ECHO] " __VA_ARGS__)
#else
#define ECHO(...) ((void)0)
#endif

#define HEALTH_BEAT_EVERY_MS(id, last_ms_var, period_ms)                                           \
    do {                                                                                           \
        uint32_t __now = to_ms_since_boot(get_absolute_time());                                    \
        if (((__now) - (last_ms_var)) >= (period_ms)) {                                            \
            (last_ms_var) = __now;                                                                 \
            Health_Heartbeat((id));                                                                \
        }                                                                                          \
    } while (0)

/********************************************************************************
 *                          PERIPHERAL ASSIGNMENTS                              *
 ********************************************************************************/
// I2C Peripheral Assignments
#define I2C_SPEED 100000                            // 100 kHz standard mode
#define EEPROM_I2C i2c1                             // Using I2C1 for EEPROM communication
#define MCP23017_RELAY_I2C i2c1                     // Using I2C1 for Relay Board MCP23017
#define MCP23017_DISPLAY_I2C i2c0                   // Using I2C0 for Display Board MCP23017
#define MCP23017_SELECTION_I2C MCP23017_DISPLAY_I2C // Using I2C0 for Selection Row MCP23017

// SPI Peripheral Assignments
#define SPI_SPEED_W5500 40000000 // 40MHz
#define W5500_SPI_INSTANCE spi0  // SPI0 for W5500 Ethernet Modul

/********************************************************************************
 *                            CONSOLE CONFIGURATIONS                            *
 ********************************************************************************/

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_CMD_BUF_LEN 1024
#define UART_MAX_LINES 4

/********************************************************************************
 *                           HLW8032 UART CHANNELS                              *
 ********************************************************************************/

#define HLW8032_UART_ID uart0
#define HLW8032_BAUDRATE 4800
#define HLW8032_FRAME_LENGTH 24
#define NUM_CHANNELS 8
#define POLL_INTERVAL_MS 6000u
#define TX_CH1 0
#define TX_CH2 1
#define TX_CH3 2
#define TX_CH4 3
#define TX_CH5 4
#define TX_CH6 5
#define TX_CH7 6
#define TX_CH8 7

/********************************************************************************
 *                       MCU SPECIFIC DEFINES                                   *
 ********************************************************************************/
#define VREG_BASE 0x40064000
#define VREG_VSEL_MASK 0x7

/********************************************************************************
 *                        RP2040 GPIO PIN ASSIGNMENTS                           *
 ********************************************************************************/
#define UART0_RX 0
#define UART0_TX 1
#define I2C1_SDA 2
#define I2C1_SCL 3
#define I2C0_SDA 4
#define I2C0_SCL 5
#define GPIO6 6 // Not used anymore
#define GPIO7 7 // Not used anymore
#define UART1_TX 8
#define UART1_RX 9
#define GPIO10 10 // Not used anymore
#define GPIO11 11 // Not used anymore
#define KEY_0 12
#define KEY_1 13
#define KEY_2 14
#define KEY_3 15
#define BUT_PLUS KEY_1
#define BUT_MINUS KEY_0
#define BUT_SET KEY_2
#define BUT_PWR KEY_3
#define W5500_MISO 16
#define W5500_CS 17
#define W5500_SCK 18
#define W5500_MOSI 19
#define W5500_RESET 20
#define W5500_INT 21
#define GPIO22 22 // Not used anymore
#define MCP_MB_RST 23
#define MCP_DP_RST 24
#define VREG_EN 25
#define ADC_VUSB 26
#define NC 27
#define PROC_LED 28
#define ADC_12V_MEA 29

/********************************************************************************
 *                         ADC CHANNEL ASSIGNMENTS                              *
 ********************************************************************************/
#define ADC_VREF 3.30f
#define ADC_MAX 4096.0f
#define ADC_TOL 1.005f // +0.5% correction factor
#define V_USB 0
#define V_SUPPLY 3
#define TEMP_SENSOR 4
#define VBUS_DIVIDER 2.0f
#define SUPPLY_DIVIDER 10.0f

/********************************************************************************
 *                       MCP23017 RELAY BOARD CONFIGURATIONS                    *
 ********************************************************************************/
#define MCP_RELAY_ADDR 0x20 // 0b0100000
#define REL_0 0
#define REL_1 1
#define REL_2 2
#define REL_3 3
#define REL_4 4
#define REL_5 5
#define REL_6 6
#define REL_7 7
#define MUX_A 8
#define MUX_B 9
#define MUX_C 10
#define MUX_EN 11

/********************************************************************************
 *                        MCP23017 DISPLAY BOARD CONFIGURATIONS                 *
 ********************************************************************************/
#define MCP_DISPLAY_ADDR 0x21 // 0b0100001
#define OUT_1 0
#define OUT_2 1
#define OUT_3 2
#define OUT_4 3
#define OUT_5 4
#define OUT_6 5
#define OUT_7 6
#define OUT_8 7
#define FAULT_LED 8
#define ETH_LED 9
#define PWR_LED 10

/********************************************************************************
 *                        MCP23017 SELECTION ROW CONFIGURATIONS                 *
 ********************************************************************************/

#define MCP_SELECTION_ADDR 0x23
#define SEL_1 0
#define SEL_2 1
#define SEL_3 2
#define SEL_4 3
#define SEL_5 4
#define SEL_6 5
#define SEL_7 6
#define SEL_8 7

#endif /* CONFIG_H */

/** @} */
/** @} */