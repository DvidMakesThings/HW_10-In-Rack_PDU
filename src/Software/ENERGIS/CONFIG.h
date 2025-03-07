#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// I2C Peripheral Assignments
#define EEPROM_I2C         		i2c1  ///< Using I2C1 for EEPROM communication
#define MCP23017_RELAY_I2C 		i2c1  ///< Using I2C0 for Relay Board MCP23017
#define MCP23017_DISPLAY_I2C 	i2c0 ///< Using I2C1 for Display Board MCP23017

// SPI Peripheral Assignments
#define SPI_SPEED				40000000	//62.5MHz
#define ILI9488_SPI_INSTANCE 	spi1  ///< SPI1 for ILI9488 Display
#define W5500_SPI_INSTANCE   	spi0  ///< SPI0 for W5500 Ethernet Modul

// Maximum number of sockets supported by the driver.
#define MAX_SOCKETS 8
// Socket that is used for the HTTP server (can be any 0..MAX_SOCKETS-1)
#define HTTP_SOCKET 0
// Socket protocols (currently only TCP is implemented)
#define SOCKET_MR_TCP 0x01

// --- Buffer Sizes and Socket Memory Mapping ---
// (Assume each socket has 2KB TX and RX buffers)
#define TX_BUF_SIZE 2048
#define RX_BUF_SIZE 2048

// Calculate the base register address for a socket.
// According to the W5500 datasheet, each socket has a 0x100 register block.
#define SOCKET_REG_BASE(sock) (0x4000 + ((sock) * 0x0100))

// Calculate the base addresses for each socket's TX and RX buffers.
// (These addresses are chosen for this example and must match your W5500 configuration.)
#define SOCKET_TX_BUF_BASE(sock) (0x8000 + ((sock) * TX_BUF_SIZE))
#define SOCKET_RX_BUF_BASE(sock) (0xC000 + ((sock) * RX_BUF_SIZE))

// --- W5500 SPI Operation Macros ---
#define W5500_VDM      0x00  // Variable Data Length Mode
#define W5500_READ_OP  0x00
#define W5500_WRITE_OP 0x04

// --- Socket Register Offsets ---
#define Sn_MR      0x0000  // Mode Register
#define Sn_CR      0x0001  // Command Register
#define Sn_IR      0x0002  // Interrupt Register
#define Sn_SR      0x0003  // Status Register
#define Sn_PORT    0x0004  // Source Port (2 bytes)
#define Sn_TX_FSR  0x0020  // TX Free Size Register (2 bytes)
#define Sn_TX_WR   0x0024  // TX Write Pointer Register (2 bytes)
#define Sn_RX_RSR  0x0026  // RX Received Size Register (2 bytes)
#define Sn_RX_RD   0x0028  // RX Read Pointer Register (2 bytes)

// Socket Commands
#define SOCKET_CMD_OPEN   0x01
#define SOCKET_CMD_LISTEN 0x02
#define SOCKET_CMD_SEND   0x20
#define SOCKET_CMD_RECV   0x40
#define SOCKET_CMD_CLOSE  0x10

// Socket Status Values
#define SOCK_CLOSED      0x00
#define SOCK_INIT        0x13
#define SOCK_LISTEN      0x14
#define SOCK_ESTABLISHED 0x17


// RP2040 GPIO Pin Assignments
#define UART0_RX        0
#define UART0_TX        1

#define I2C1_SDA        2
#define I2C1_SCL        3
#define I2C0_SDA        4
#define I2C0_SCL        5

#define LCD_DC          6
#define LCD_BL          7
#define LCD_MISO        8   // spi1 rx
#define LCD_CS          9   // spi1 csn
#define LCD_SCLK        10  // spi1sck
#define LCD_MOSI        11  // spi1 tx
#define LCD_RESET       22

#define KEY_0           12
#define KEY_1           13
#define KEY_2           14
#define KEY_3           15

#define W5500_MISO      16
#define W5500_CS        17
#define W5500_SCK       18
#define W5500_MOSI      19
#define W5500_RESET     20
#define W5500_INT       21

#define MCP_REL_RST     23
#define MCP_LCD_RST     24

#define ADC_VUSB        26
#define ADC_12V_MEA     29

// MCP23017 on Relay Board (I2C1)
#define MCP_RELAY_ADDR  0x20  // 0b0100000
#define REL_0           0
#define REL_1           1
#define REL_2           2
#define REL_3           3
#define REL_4           4
#define REL_5           5
#define REL_6           6
#define REL_7           7
#define MUX_A           8
#define MUX_B           9
#define MUX_C           10
#define MUX_EN          11

// MCP23017 on Display Board (I2C0)
#define MCP_DISPLAY_ADDR 0x21 // 0b0100001
#define OUT_1           0
#define OUT_2           1
#define OUT_3           2
#define OUT_4           3
#define OUT_5           4
#define OUT_6           5
#define OUT_7           6
#define OUT_8           7
#define FAULT_LED       8
#define ETH_LED         9
#define PWR_LED         10

// SN74HC151 (MUX for UART TX Channels)
#define TX_MUX_1        0
#define TX_MUX_2        1
#define TX_MUX_3        2
#define TX_MUX_4        3
#define TX_MUX_5        4
#define TX_MUX_6        5
#define TX_MUX_7        6
#define TX_MUX_8        7
#define UART_MUX_Y      UART0_TX
#define MUX_SELECT_A    MUX_A
#define MUX_SELECT_B    MUX_B
#define MUX_SELECT_C    MUX_C
#define MUX_ENABLE      MUX_EN

// HLW8032 UART Channels via Optocouplers
#define TX_CH1          0
#define TX_CH2          1
#define TX_CH3          2
#define TX_CH4          3
#define TX_CH5          4
#define TX_CH6          5
#define TX_CH7          6
#define TX_CH8          7

// ILI9488 Command Set
#define ILI9488_CMD_SLEEP_OUT    0x11
#define ILI9488_CMD_DISPLAY_ON   0x29
#define ILI9488_CMD_COLUMN_ADDR  0x2A
#define ILI9488_CMD_PAGE_ADDR    0x2B
#define ILI9488_CMD_MEMORY_WRITE 0x2C

#define ILI9488_WIDTH  480  ///< Display width in pixels
#define ILI9488_HEIGHT 320  ///< Display height in pixels

// Use 24-bit RGB (R, G, B)
#define COLOR_BLUE       0x0000FC  // (0, 0, 252)
#define COLOR_CYAN       0x00FCFC  // (0, 252, 252)
#define COLOR_GREEN      0x00FC00  // (0, 252, 0)
#define COLOR_YELLOW     0xFCFC00  // (252, 252, 0)
#define COLOR_ORANGE     0xFC8000  // (252, 128, 0)
#define COLOR_RED        0xFC0000  // (252, 0, 0)
#define COLOR_MAGENTA    0xFC00FC  // (252, 0, 252)
#define COLOR_PURPLE     0x8000FC  // (128, 0, 252)
#define COLOR_BLACK      0x000000  // (0, 0, 0)
#define COLOR_WHITE      0xFCFCFC  // (252, 252, 252)

// UI-Specific Colors (Keep these as 16-bit if needed)
#define COLOR_DARK_BLUE    0x000066
#define COLOR_DARK_GRAY    0x050505  // Converted to 24-bit (48, 48, 48)
#define COLOR_LIGHT_GRAY   0x303030  // Converted to 24-bit (128, 128, 128)

#define COLOR_BG_PRIMARY   COLOR_DARK_GRAY
#define COLOR_BG_SECONDARY COLOR_BLACK
#define COLOR_TEXT_PRIMARY COLOR_WHITE
#define COLOR_TEXT_SECONDARY COLOR_LIGHT_GRAY
#define COLOR_HIGHLIGHT    COLOR_BLUE
#define COLOR_WARNING      COLOR_YELLOW
#define COLOR_ERROR        COLOR_RED

// MCP23017 Registers
#define MCP23017_IODIRA   0x00 // I/O Direction Register A
#define MCP23017_IODIRB   0x01 // I/O Direction Register B
#define MCP23017_GPIOA    0x12 // GPIO Register A
#define MCP23017_GPIOB    0x13 // GPIO Register B
#define MCP23017_OLATA    0x14 // Output Latch Register A
#define MCP23017_OLATB    0x15 // Output Latch Register B

// EEPROM Configuration
#define CAT24C512_I2C_ADDR  0x50  ///< 7-bit I2C address of the EEPROM
#define EEPROM_SIZE         65536 ///< Total EEPROM size in bytes
#define CAT24C512_PAGE_SIZE 128   ///< EEPROM page write limit

#endif // CONFIG_H