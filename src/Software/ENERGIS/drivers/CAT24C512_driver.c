/**
 * @file CAT24C512_driver.c
 * @author David Sipos
 * @brief Implementation of CAT24C512 I2C EEPROM Driver
 * @version 1.0
 * @date 2025-03-03
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
 */
void CAT24C512_Init(void) {
    i2c_init(EEPROM_I2C, 400000);        // 400 kHz Fast Mode
    gpio_set_function(2, GPIO_FUNC_I2C); // SDA (adjust based on config)
    gpio_set_function(3, GPIO_FUNC_I2C); // SCL (adjust based on config)
    gpio_pull_up(2);
    gpio_pull_up(3);
}

/**
 * @brief Writes a byte to the EEPROM.
 * @param addr 16-bit memory address.
 * @param data Byte to write.
 * @return 0 if successful, -1 if failed.
 */
int CAT24C512_WriteByte(uint16_t addr, uint8_t data) {
    uint8_t buffer[3] = {addr >> 8, addr & 0xFF, data};

    int res = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, 3, false);
    sleep_ms(5); // Write cycle delay

    return (res == 3) ? 0 : -1;
}

/**
 * @brief Reads a byte from the EEPROM.
 * @param addr 16-bit memory address.
 * @return The read byte.
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
 * @param addr 16-bit memory address.
 * @param data Pointer to data buffer.
 * @param len Number of bytes to write.
 * @return 0 if successful, -1 if failed.
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
 * @param addr 16-bit memory address.
 * @param buffer Pointer to buffer to store data.
 * @param len Number of bytes to read.
 */
void CAT24C512_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len) {
    uint8_t addr_buf[2] = {addr >> 8, addr & 0xFF};

    i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, addr_buf, 2, true);
    i2c_read_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, len, false);
}

/**
 * @brief Dumps the entire EEPROM contents into a buffer.
 * @param buffer Pointer to a 64 KB buffer (EEPROM size).
 */
void CAT24C512_Dump(uint8_t *buffer) {
    CAT24C512_ReadBuffer(0x0000, buffer, 65536); // Read entire EEPROM (64KB)
}

/**
 * @brief Dumps the EEPROM contents in a formatted hex table.
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