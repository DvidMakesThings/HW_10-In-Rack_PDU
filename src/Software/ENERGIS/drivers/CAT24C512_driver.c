/**
 * @file CAT24C512_driver.c
 * @author DvidMakesThings - David Sipos
 * @defgroup driver2 2. CAT24C512 EEPROM Driver
 * @ingroup drivers
 * @brief Implementation of CAT24C512 I2C EEPROM Driver
 * @{
 * @version 1.0
 * @date 2025-03-03
 *
 * This file implements the driver for CAT24C512 EEPROM, a 512Kbit (64KB) I2C EEPROM.
 * The driver provides functions for initialization, reading and writing data, and
 * utility functions for debugging EEPROM contents.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "CAT24C512_driver.h"
#include "../CONFIG.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Initializes I2C1 for EEPROM communication.
 *
 * This function configures the I2C1 interface with a 400 kHz clock frequency (Fast Mode)
 * for communication with the CAT24C512 EEPROM. It also sets up GPIO pins 2 and 3 as SDA and SCL
 * respectively, and enables pull-up resistors on these pins.
 */
void CAT24C512_Init(void) {
    i2c_init(EEPROM_I2C, I2C_SPEED);     // 400 kHz Fast Mode
    gpio_set_function(2, GPIO_FUNC_I2C); // SDA (adjust based on config)
    gpio_set_function(3, GPIO_FUNC_I2C); // SCL (adjust based on config)
    gpio_pull_up(2);
    gpio_pull_up(3);
}

/**
 * @brief Writes a byte to the EEPROM.
 *
 * This function writes a single byte to the specified address in the EEPROM.
 * It handles the 16-bit addressing required by the CAT24C512 and automatically
 * introduces a 5ms delay to allow the write cycle to complete.
 *
 * @param addr 16-bit memory address (0x0000 - 0xFFFF).
 * @param data Byte to write to the specified address.
 * @return 0 if successful, -1 if the I2C write operation failed.
 */
int CAT24C512_WriteByte(uint16_t addr, uint8_t data) {
    uint8_t buffer[3] = {addr >> 8, addr & 0xFF, data};

    int res = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, 3, false);
    sleep_ms(5); // Write cycle delay

    return (res == 3) ? 0 : -1;
}

/**
 * @brief Reads a byte from the EEPROM.
 *
 * This function reads a single byte from the specified address in the EEPROM.
 * It first sends the 16-bit address to the EEPROM followed by a repeated start
 * condition, then reads the byte from the device.
 *
 * @param addr 16-bit memory address (0x0000 - 0xFFFF).
 * @return The byte read from the specified address.
 */
uint8_t CAT24C512_ReadByte(uint16_t addr) {
    uint8_t addr_buf[2] = {addr >> 8, addr & 0xFF};
    uint8_t data;

    i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, addr_buf, 2, true);
    i2c_read_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, &data, 1, false);

    return data;
}

/**
 * @brief Writes a buffer of data to the EEPROM.
 *
 * This function writes a sequence of bytes to the EEPROM starting at the specified address.
 * It automatically handles page boundaries by breaking the write operation into multiple
 * chunks of CAT24C512_PAGE_SIZE (typically 128 bytes) or less. A 5ms delay is added after
 * each page write to allow the write cycle to complete.
 *
 * @param addr 16-bit starting memory address (0x0000 - 0xFFFF).
 * @param data Pointer to the source data buffer.
 * @param len Number of bytes to write.
 * @return 0 if successful, -1 if any I2C write operation failed.
 */
int CAT24C512_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len) {
    while (len > 0) {
        uint16_t chunk_size = (len > CAT24C512_PAGE_SIZE) ? CAT24C512_PAGE_SIZE : len;
        uint8_t buffer[CAT24C512_PAGE_SIZE + 2];
        buffer[0] = addr >> 8;
        buffer[1] = addr & 0xFF;
        memcpy(&buffer[2], data, chunk_size);

        int res = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, chunk_size + 2, false);
        if (res != (chunk_size + 2))
            return -1;

        sleep_ms(5); // Write cycle delay
        addr += chunk_size;
        data += chunk_size;
        len -= chunk_size;
    }
    return 0;
}

/**
 * @brief Reads a buffer of data from the EEPROM.
 *
 * This function reads a sequence of bytes from the EEPROM starting at the specified address.
 * It first sends the 16-bit address to the EEPROM followed by a repeated start condition,
 * then reads the requested number of bytes into the provided buffer.
 *
 * @param addr 16-bit starting memory address (0x0000 - 0xFFFF).
 * @param buffer Pointer to the destination buffer where read data will be stored.
 * @param len Number of bytes to read (buffer must be at least this size).
 */
void CAT24C512_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len) {
    uint8_t addr_buf[2] = {addr >> 8, addr & 0xFF};

    i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, addr_buf, 2, true);
    i2c_read_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, len, false);
}

/**
 * @brief Dumps the entire EEPROM contents into a buffer.
 *
 * This function reads the entire 64KB of the EEPROM memory and stores it in the provided buffer.
 * This can be useful for backing up EEPROM contents or for debugging purposes.
 *
 * @param buffer Pointer to a buffer of at least 65536 bytes (64KB) to store the EEPROM contents.
 *               The caller must ensure the buffer is large enough.
 */
void CAT24C512_Dump(uint8_t *buffer) {
    CAT24C512_ReadBuffer(0x0000, buffer, 65536); // Read entire EEPROM (64KB)
}

/**
 * @brief Dumps the EEPROM contents in a formatted hex table.
 *
 * This function reads the entire EEPROM memory and outputs its contents as a formatted
 * hexadecimal table to the standard output (typically UART). The output is organized
 * as 16-byte rows with address headers and column headers for easier reading.
 * The output is delimited by "EE_DUMP_START" and "EE_DUMP_END" markers.
 *
 * Output format example:
 * ```
 * EE_DUMP_START
 * Addr    00      01      02      03      04      05      06      07      08      09      0A 0B 0C
 * 0D      0E      0F 0x0000  FF      FF      FF      FF      FF      FF      FF      FF      FF FF
 * FF      FF      FF      FF      FF      FF 0x0010  FF      FF      FF      FF      FF      FF FF
 * FF      FF      FF      FF      FF      FF      FF      FF      FF
 * ...
 * EE_DUMP_END
 * ```
 */
void CAT24C512_DumpFormatted(void) {
    char line[256];
    char field[16];

    printf("EE_DUMP_START\n");
    // Print header row
    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%-7s", "Addr");
    for (uint8_t col = 0; col < 16; col++) {
        char colStr[8];
        sprintf(colStr, "%02X", col);
        sprintf(field, "%-7s", colStr);
        strcat(line, field);
    }
    printf("%s\n", line);

    // Dump EEPROM contents in 16-byte blocks using a 32-bit loop counter
    for (uint32_t addr = 0; addr < EEPROM_SIZE; addr += 16) {
        memset(line, 0, sizeof(line));
        sprintf(field, "0x%04X", (uint16_t)addr);
        sprintf(line, "%-7s", field);
        uint8_t buffer[16];
        CAT24C512_ReadBuffer(addr, buffer, 16);
        for (uint8_t i = 0; i < 16; i++) {
            sprintf(field, "%02X     ", buffer[i]);
            strcat(line, field);
        }
        printf("%s\n", line);
    }
    printf("EE_DUMP_END\n");
}