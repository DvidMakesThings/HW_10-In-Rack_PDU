/**
 * @file CONFIG.h
 * @author DvidMakesThings - David Sipos
 * @brief Configuration file for the ENERGIS PDU project.
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "hardware/adc.h"
#include "hardware/address_mapped.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "misc/uart_command_handler.h"
#include "network/snmp_custom.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ENERGIS_core1_task.h"

// Wiznet IoLibrary headers
#include "network/socket.h"
#include "network/w5x00_spi.h"
#include "network/wizchip_conf.h"

// Drivers & utilities
#include "ENERGIS_startup.h"
#include "PDU_display.h"
#include "drivers/CAT24C512_driver.h"
#include "drivers/HLW8032_driver.h"
#include "drivers/ILI9488_driver.h"
#include "drivers/MCP23017_display_driver.h"
#include "drivers/MCP23017_dual_driver.h"
#include "drivers/MCP23017_relay_driver.h"
#include "drivers/MCP23017_selection_driver.h"
#include "utils/EEPROM_MemoryMap.h"
#include "utils/helper_functions.h"
#include "web_handlers/form_helpers.h"

/********************************************************************************
 *                          GLOBAL CONFIGURATIONS                               *
 *                          CONFIGURABLE BY USER                                *
 ********************************************************************************/

/*********************** Define if the device has a screen **********************/
#define HAS_SCREEN 0

/********************** Button behavior for press durations *********************/
// Button longpress duration thresholds
#define LONGPRESS_DT 2500
// Debounce time for button presses
#define DEBOUNCE_MS 100u
#define POST_GUARD_MS (DEBOUNCE_MS + 10u)
/****************************** Guardian functions ******************************/
// poll cadence
#define DUAL_GUARD_INTERVAL_MS 50u
// monitor A-port only (IO0..7). Change to 0xFFFF if all 16 needed
#define DUAL_GUARD_MASK 0x00FFu
// 1 = fix mismatches by writing both chips to relay's value
#define DUAL_GUARD_AUTOHEAL 1

/********************************************************************************
 *                          GLOBAL CONFIGURATIONS                               *
 *                       DO NOT TOUCH PART FROM HERE                            *
 ********************************************************************************/

// Default values (Factory settings)
#define DEFAULT_SN "SN-369366060325"
#define SWVERSION "1.0.0"
#define SW_REV 100 // 1.0.0 → numeric literal
#define DEFAULT_NAME "ENERGIS-1.0.0"
#define DEFAULT_LOCATION "Location"

/* log.h — single source of truth */
#pragma once

/* Allow -D overrides but make behavior compile-time */
#ifndef DEBUG
#define DEBUG 0
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
#define PLOT_EN 1
#endif
#ifndef NETLOG
#define NETLOG 0
#endif
#ifndef UART_IFACE
#define UART_IFACE 1
#endif

#if DEBUG
#define DEBUG_PRINT(...) printf("\t[DEBUG] " __VA_ARGS__)
#else
#define DEBUG_PRINT(...) ((void)0)
#endif

#if INFO
#define INFO_PRINT(...) printf("[INFO] " __VA_ARGS__)
#else
#define INFO_PRINT(...) ((void)0)
#endif

#if ERROR
#define ERROR_PRINT(...) printf("[ERROR] " __VA_ARGS__)
#else
#define ERROR_PRINT(...) ((void)0)
#endif

#if WARNING
#define WARNING_PRINT(...) printf("[WARNING] " __VA_ARGS__)
#else
#define WARNING_PRINT(...) ((void)0)
#endif

#if PLOT_EN
#define PLOT(...) printf("\t[PLOT] " __VA_ARGS__)
#else
#define PLOT(...) ((void)0)
#endif

#if NETLOG
#define NETLOG_PRINT(...) printf("[NETLOG] " __VA_ARGS__)
#else
#define NETLOG_PRINT(...) ((void)0)
#endif

#if UART_IFACE
#define ECHO(...) printf("\t[ECHO] " __VA_ARGS__)
#else
#define ECHO(...) ((void)0)
#endif

// I2C Peripheral Assignments
#define EEPROM_I2C i2c1                             ///< Using I2C1 for EEPROM communication
#define MCP23017_RELAY_I2C i2c1                     ///< Using I2C0 for Relay Board MCP23017
#define MCP23017_DISPLAY_I2C i2c0                   ///< Using I2C1 for Display Board MCP23017
#define MCP23017_SELECTION_I2C MCP23017_DISPLAY_I2C ///< Using I2C1 for Selection Row MCP23017

// SPI Peripheral Assignments
#if HAS_SCREEN
#define SPI_SPEED 62500000 // 62.5MHz
#endif

#define SPI_SPEED_W5500 40000000  // 40MHz
#define ILI9488_SPI_INSTANCE spi1 ///< SPI1 for ILI9488 Display
#define W5500_SPI_INSTANCE spi0   ///< SPI0 for W5500 Ethernet Modul

/********************************************************************************
 *                            UART CONFIGURATIONS                               *
 ********************************************************************************/

#define UART_ID uart1
#define BAUD_RATE 115200
#define UART_CMD_BUF_LEN 1024
#define UART_MAX_LINES 4

/********************************************************************************
 *                       HLW8032 UART CONFIGURATIONS                            *
 ********************************************************************************/
#define HLW8032_UART_ID uart0
#define HLW8032_BAUDRATE 4800
#define HLW8032_FRAME_LENGTH 24
#define NUM_CHANNELS 8
#define POLL_INTERVAL_MS 6000u

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
#define LCD_DC 6
#define LCD_BL 7
#define LCD_MISO 8  // spi1 rx
#define LCD_CS 9    // spi1 csn
#define LCD_SCLK 10 // spi1 sck
#define LCD_MOSI 11 // spi1 tx
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
#define LCD_RESET 22
#define MCP_REL_RST 23
#define MCP_LCD_RST 24
#define VREG_EN 25
#define ADC_VUSB 26
#define NC 27
#define PROC_LED 28
#define ADC_12V_MEA 29

/********************************************************************************
 *                         ADC CHANNEL ASSIGNMENTS                              *
 ********************************************************************************/
#define ADC_VREF 3.00f
#define ADC_MAX 4096.0f
#define ADC_TOL 1.005f // +0.5% correction factor
#define V_USB 0
#define V_SUPPLY 3
#define TEMP_SENSOR 4

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
#define MCP_SEL_RST MCP_LCD_RST
#define SEL_1 0
#define SEL_2 1
#define SEL_3 2
#define SEL_4 3
#define SEL_5 4
#define SEL_6 5
#define SEL_7 6
#define SEL_8 7

/********************************************************************************
 *                        SN74HC151 (MUX for UART TX Channels)                  *
 ********************************************************************************/
#define TX_MUX_1 0
#define TX_MUX_2 1
#define TX_MUX_3 2
#define TX_MUX_4 3
#define TX_MUX_5 4
#define TX_MUX_6 5
#define TX_MUX_7 6
#define TX_MUX_8 7
#define UART_MUX_Y UART0_TX
#define MUX_SELECT_A MUX_A
#define MUX_SELECT_B MUX_B
#define MUX_SELECT_C MUX_C
#define MUX_ENABLE MUX_EN

// HLW8032 UART Channels via Optocouplers
/********************************************************************************
 *                           HLW8032 UART CHANNELS                              *
 ********************************************************************************/
#define TX_CH1 0
#define TX_CH2 1
#define TX_CH3 2
#define TX_CH4 3
#define TX_CH5 4
#define TX_CH6 5
#define TX_CH7 6
#define TX_CH8 7

/********************************************************************************
 *                         LCD REGISTERS AND CONFIGURATION                      *
 ********************************************************************************/
#if HAS_SCREEN
// ILI9488 Command Set
#define ILI9488_CMD_SLEEP_OUT 0x11
#define ILI9488_CMD_DISPLAY_ON 0x29
#define ILI9488_CMD_COLUMN_ADDR 0x2A
#define ILI9488_CMD_PAGE_ADDR 0x2B
#define ILI9488_CMD_MEMORY_WRITE 0x2C

#define ILI9488_WIDTH 480  ///< Display width in pixels
#define ILI9488_HEIGHT 320 ///< Display height in pixels

// Use 24-bit RGB (R, G, B)
#define COLOR_BLUE 0x0000FC    // (0, 0, 252)
#define COLOR_CYAN 0x00FCFC    // (0, 252, 252)
#define COLOR_GREEN 0x00FC00   // (0, 252, 0)
#define COLOR_YELLOW 0xFCFC00  // (252, 252, 0)
#define COLOR_ORANGE 0xFC8000  // (252, 128, 0)
#define COLOR_RED 0xFC0000     // (252, 0, 0)
#define COLOR_MAGENTA 0xFC00FC // (252, 0, 252)
#define COLOR_PURPLE 0x8000FC  // (128, 0, 252)
#define COLOR_BLACK 0x000000   // (0, 0, 0)
#define COLOR_WHITE 0xFCFCFC   // (252, 252, 252)

// UI-Specific Colors (Keep these as 16-bit if needed)
#define COLOR_DARK_BLUE 0x000066
#define COLOR_DARK_GRAY 0x050505  // Converted to 24-bit (48, 48, 48)
#define COLOR_LIGHT_GRAY 0x303030 // Converted to 24-bit (128, 128, 128)

#define COLOR_BG_PRIMARY COLOR_DARK_GRAY
#define COLOR_BG_SECONDARY COLOR_BLACK
#define COLOR_TEXT_PRIMARY COLOR_WHITE
#define COLOR_TEXT_SECONDARY COLOR_LIGHT_GRAY
#define COLOR_HIGHLIGHT COLOR_BLUE
#define COLOR_WARNING COLOR_YELLOW
#define COLOR_ERROR COLOR_RED
#endif

// MCP23017 Registers
/********************************************************************************
 *                            MCP23017 REGISTERS                                *
 ********************************************************************************/
#define MCP23017_IODIRA 0x00   // I/O Direction Register A
#define MCP23017_IODIRB 0x01   // I/O Direction Register B
#define MCP23017_IPOLA 0x02    // Input Polarity Register A
#define MCP23017_IPOLB 0x03    // Input Polarity Register B
#define MCP23017_GPINTENA 0x04 // Interrupt-on-Change Control Register A
#define MCP23017_GPINTENB 0x05 // Interrupt-on-Change Control Register B
#define MCP23017_DEFVALA 0x06  // Default Compare Register A
#define MCP23017_DEFVALB 0x07  // Default Compare Register B
#define MCP23017_INTCONA 0x08  // Interrupt Control Register A
#define MCP23017_INTCONB 0x09  // Interrupt Control Register B
#define MCP23017_IOCON 0x0A    // Configuration Register (Bank=0)
#define MCP23017_IOCONB 0x0B   // Configuration Register (Bank=1)
#define MCP23017_GPPUA 0x0C    // Pull-up Resistor Config Register A
#define MCP23017_GPPUB 0x0D    // Pull-up Resistor Config Register B
#define MCP23017_INTFA 0x0E    // Interrupt Flag Register A
#define MCP23017_INTFB 0x0F    // Interrupt Flag Register B
#define MCP23017_INTCAPA 0x10  // Interrupt Captured Value Register A
#define MCP23017_INTCAPB 0x11  // Interrupt Captured Value Register B
#define MCP23017_GPIOA 0x12    // GPIO Register A
#define MCP23017_GPIOB 0x13    // GPIO Register B
#define MCP23017_OLATA 0x14    // Output Latch Register A
#define MCP23017_OLATB 0x15    // Output Latch Register B

// EEPROM Configuration
/********************************************************************************
 *                           EEPROM CONFIGURATION                               *
 ********************************************************************************/
#define CAT24C512_I2C_ADDR 0x50 ///< 7-bit I2C address of the EEPROM
#define EEPROM_SIZE 65536       ///< Total EEPROM size in bytes
#define CAT24C512_PAGE_SIZE 128 ///< EEPROM page write limit

// Stores the currently selected row
/********************************************************************************
 *                             GLOBAL VARIABLES                                 *
 ********************************************************************************/
extern volatile uint8_t selected_row;
extern volatile bool bootsel_requested;
extern volatile bool dump_eeprom_pending;

#endif // CONFIG_H