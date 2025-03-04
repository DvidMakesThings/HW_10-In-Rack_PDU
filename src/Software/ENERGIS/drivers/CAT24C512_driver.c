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
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/flash.h"
#include "../CONFIG.h"
#include <stdio.h>
#include <string.h>
#include "../utils/ff.h"  // FatFS header

/**
 * @brief Initializes I2C1 for EEPROM communication.
 */
void CAT24C512_Init(void) {
    i2c_init(EEPROM_I2C, 400000); // 400 kHz Fast Mode
    gpio_set_function(2, GPIO_FUNC_I2C);  // SDA (adjust based on config)
    gpio_set_function(3, GPIO_FUNC_I2C);  // SCL (adjust based on config)
    gpio_pull_up(2);
    gpio_pull_up(3);
}

/**
 * @brief Writes a byte to the EEPROM.
 */
int CAT24C512_WriteByte(uint16_t addr, uint8_t data) {
    uint8_t buffer[3] = { addr >> 8, addr & 0xFF, data };

    int res = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, 3, false);
    sleep_ms(5); // Write cycle delay

    return (res == 3) ? 0 : -1;
}

/**
 * @brief Reads a byte from the EEPROM.
 */
uint8_t CAT24C512_ReadByte(uint16_t addr) {
    uint8_t addr_buf[2] = { addr >> 8, addr & 0xFF };
    uint8_t data;

    i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, addr_buf, 2, true);
    i2c_read_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, &data, 1, false);

    return data;
}

/**
 * @brief Writes a buffer of data to the EEPROM.
 */
int CAT24C512_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len) {
    while (len > 0) {
        uint16_t chunk_size = (len > CAT24C512_PAGE_SIZE) ? CAT24C512_PAGE_SIZE : len;
        uint8_t buffer[CAT24C512_PAGE_SIZE + 2];
        buffer[0] = addr >> 8;
        buffer[1] = addr & 0xFF;
        memcpy(&buffer[2], data, chunk_size);

        int res = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, chunk_size + 2, false);
        if (res != (chunk_size + 2)) return -1;

        sleep_ms(5); // Write cycle delay
        addr += chunk_size;
        data += chunk_size;
        len -= chunk_size;
    }
    return 0;
}

/**
 * @brief Reads a buffer of data from the EEPROM.
 */
void CAT24C512_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len) {
    uint8_t addr_buf[2] = { addr >> 8, addr & 0xFF };
    
    i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, addr_buf, 2, true);
    i2c_read_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, len, false);
}

/**
 * @brief Dumps the entire EEPROM contents into a buffer.
 */
void CAT24C512_Dump(uint8_t *buffer) {
    CAT24C512_ReadBuffer(0x0000, buffer, 65536); // Read entire EEPROM (64KB)
}

/**
 * @brief Dumps the EEPROM contents in a formatted hex table.
 * @param output_mode Selects where to send the data:
 *        - 0 = Save to a file on Flash
 *        - 1 = Send via Serial (UART)
 */
/**
 * @brief Dumps the EEPROM contents in a formatted hex table with a header row.
 * @param output_mode Selects where to send the data:
 *        - 0 = Save to a file on Flash
 *        - 1 = Send via Serial (UART)
 */
/**
 * @brief Dumps the EEPROM contents in a formatted hex table with a header row.
 *        The header columns are aligned with the data fields below.
 * @param output_mode Selects where to send the data:
 *        - 0 = Save to a file on Flash
 *        - 1 = Send via Serial (UART)
 */


/**
 * @brief Dumps the EEPROM contents in a formatted hex table with a header row,
 *        and writes it into a file on flash (using FatFS).
 *        The header columns are aligned with the data fields below.
 * @param output_mode Selects where to send the data:
 *        - 0 = Save to a file on Flash (FatFS)
 *        - 1 = Send via Serial (UART)
 */
void CAT24C512_DumpFormatted(uint8_t output_mode) {
    char line[256];
    char field[16];

    if (output_mode == 1) {
        // --- Output via Serial (UART) ---
        memset(line, 0, sizeof(line));
        snprintf(line, sizeof(line), "%-7s", "Addr");
        for (uint8_t col = 0; col < 16; col++) {
            char temp[8];
            sprintf(temp, "%02X", col);
            sprintf(field, "%-7s", temp);
            strcat(line, field);
        }
        printf("%s\n", line);

        // Dump EEPROM contents in 16-byte chunks.
        for (uint16_t addr = 0; addr < 65536; addr += 16) {
            memset(line, 0, sizeof(line));
            sprintf(field, "0x%04X", addr);
            sprintf(line, "%-7s", field);
            uint8_t buffer[16];
            CAT24C512_ReadBuffer(addr, buffer, 16);
            for (uint8_t i = 0; i < 16; i++) {
                sprintf(field, "0x%02X", buffer[i]);
                char formatted_field[16];
                sprintf(formatted_field, "%-7s", field);
                strcat(line, formatted_field);
            }
            printf("%s\n", line);
        }
    } else {
        // --- Output to a file on flash (using FatFS) ---
        FIL file;
        FRESULT res = f_open(&file, "0:/eeprom_dump.txt", FA_WRITE | FA_CREATE_ALWAYS);
        if (res != FR_OK) {
            printf("Failed to open file for writing: %d\n", res);
            return;
        }
        UINT bw;
        memset(line, 0, sizeof(line));
        snprintf(line, sizeof(line), "%-7s", "Addr");
        for (uint8_t col = 0; col < 16; col++) {
            char temp[8];
            sprintf(temp, "%02X", col);
            sprintf(field, "%-7s", temp);
            strcat(line, field);
        }
        strcat(line, "\r\n");
        f_write(&file, line, strlen(line), &bw);

        for (uint16_t addr = 0; addr < 65536; addr += 16) {
            memset(line, 0, sizeof(line));
            sprintf(field, "0x%04X", addr);
            sprintf(line, "%-7s", field);
            uint8_t buffer[16];
            CAT24C512_ReadBuffer(addr, buffer, 16);
            for (uint8_t i = 0; i < 16; i++) {
                sprintf(field, "0x%02X", buffer[i]);
                char formatted_field[16];
                sprintf(formatted_field, "%-7s", field);
                strcat(line, formatted_field);
            }
            strcat(line, "\r\n");
            f_write(&file, line, strlen(line), &bw);
        }
        f_close(&file);
        printf("EEPROM dump written to file: 0:/eeprom_dump.txt\n");
    }
}
