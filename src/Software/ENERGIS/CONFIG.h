#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define DEBUG 1
#define DEBUG_PRINT(...)                                                                           \
    if (DEBUG)                                                                                     \
        printf("[DEBUG] " __VA_ARGS__);
#define INFO 1
#define INFO_PRINT(...)                                                                            \
    if (INFO)                                                                                      \
        printf("[INFO] " __VA_ARGS__);
#define ERROR 1
#define ERROR_PRINT(...)                                                                           \
    if (ERROR)                                                                                     \
        printf("[ERROR] " __VA_ARGS__);
#define WARNING 1
#define WARNING_PRINT(...)                                                                         \
    if (WARNING)                                                                                   \
        printf("[WARNING] " __VA_ARGS__);

// I2C Peripheral Assignments
#define EEPROM_I2C i2c1           ///< Using I2C1 for EEPROM communication
#define MCP23017_RELAY_I2C i2c1   ///< Using I2C0 for Relay Board MCP23017
#define MCP23017_DISPLAY_I2C i2c0 ///< Using I2C1 for Display Board MCP23017

// SPI Peripheral Assignments
#define SPI_SPEED 62500000        // 62.5MHz
#define SPI_SPEED_W5500 40000000  // 10MHz
#define ILI9488_SPI_INSTANCE spi1 ///< SPI1 for ILI9488 Display
#define W5500_SPI_INSTANCE spi0   ///< SPI0 for W5500 Ethernet Modul

// UART Peripheral Assignments
#define UART_ID uart0
#define BAUD_RATE 115200
#define UART_CMD_BUF_LEN 256

// RP2040 GPIO Pin Assignments
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
#define LCD_SCLK 10 // spi1sck
#define LCD_MOSI 11 // spi1 tx
#define LCD_RESET 22

#define KEY_0 12
#define KEY_1 13
#define KEY_2 14
#define KEY_3 15
#define BUT_PLUS KEY_0
#define BUT_MINUS KEY_1
#define BUT_SET KEY_2
#define BUT_PWR KEY_3

#define W5500_MISO 16
#define W5500_CS 17
#define W5500_SCK 18
#define W5500_MOSI 19
#define W5500_RESET 20
#define W5500_INT 21

#define MCP_REL_RST 23
#define MCP_LCD_RST 24

#define ADC_VUSB 26
#define ADC_12V_MEA 29

// MCP23017 on Relay Board (I2C1)
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

// MCP23017 on Display Board (I2C0)
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

// SN74HC151 (MUX for UART TX Channels)
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
#define TX_CH1 0
#define TX_CH2 1
#define TX_CH3 2
#define TX_CH4 3
#define TX_CH5 4
#define TX_CH6 5
#define TX_CH7 6
#define TX_CH8 7

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

// MCP23017 Registers
#define MCP23017_IODIRA 0x00 // I/O Direction Register A
#define MCP23017_IODIRB 0x01 // I/O Direction Register B
#define MCP23017_GPIOA 0x12  // GPIO Register A
#define MCP23017_GPIOB 0x13  // GPIO Register B
#define MCP23017_OLATA 0x14  // Output Latch Register A
#define MCP23017_OLATB 0x15  // Output Latch Register B

// EEPROM Configuration
#define CAT24C512_I2C_ADDR 0x50 ///< 7-bit I2C address of the EEPROM
#define EEPROM_SIZE 65536       ///< Total EEPROM size in bytes
#define CAT24C512_PAGE_SIZE 128 ///< EEPROM page write limit

// Stores the currently selected row
extern volatile uint8_t selected_row;
extern volatile bool bootsel_requested;

#endif // CONFIG_H