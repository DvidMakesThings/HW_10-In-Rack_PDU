/**
 * @file src/drivers/CAT24C512_driver.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.0
 * @date 2025-11-06
 *
 * @details
 * RTOS-compatible implementation for CAT24C512 EEPROM.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* ==================== Private Functions ==================== */

/**
 * @brief Implements RTOS-compatible write cycle delay for EEPROM operations
 *
 * Provides the mandatory delay between write operations to ensure data integrity.
 * Uses RTOS task delay instead of busy-wait to allow other tasks to run.
 *
 * @param None
 * @return None
 * @note Critical timing: Derived from CAT24C512 datasheet write cycle specification
 */
static inline void write_cycle_delay(void) { vTaskDelay(pdMS_TO_TICKS(CAT24C512_WRITE_CYCLE_MS)); }

/* ==================== Public Functions ==================== */

void CAT24C512_Init(void) {
    /* I2C peripheral already initialized by system_startup_init() */
    /* Just set GPIO function and pull-ups using CONFIG.h definitions */

    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);

    INFO_PRINT("[CAT24C512] Driver initialized (I2C1: SDA=%u, SCL=%u)\r\n", I2C1_SDA, I2C1_SCL);
}

int CAT24C512_WriteByte(uint16_t addr, uint8_t data) {
    uint8_t buffer[3] = {addr >> 8, addr & 0xFF, data};

    int res = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, 3, false);

    if (res == 3) {
        write_cycle_delay();
        return 0;
    } else {
        ERROR_PRINT("[CAT24C512] WriteByte failed at 0x%04X\r\n", addr);
        return -1;
    }
}

uint8_t CAT24C512_ReadByte(uint16_t addr) {
    uint8_t addr_buf[2] = {addr >> 8, addr & 0xFF};
    uint8_t data = 0xFF;

    int res1 = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, addr_buf, 2, true);
    int res2 = i2c_read_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, &data, 1, false);

    if (res1 != 2 || res2 != 1) {
        ERROR_PRINT("[CAT24C512] ReadByte failed at 0x%04X\r\n", addr);
    }

    return data;
}

int CAT24C512_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len) {
    if (!data) {
        ERROR_PRINT("[CAT24C512] WriteBuffer: NULL data pointer\r\n");
        return -1;
    }

    // DEBUG_PRINT("[CAT24C512] Writing %u bytes starting at 0x%04X\r\n", len, addr);

    while (len > 0) {
        /* Calculate chunk size (respect page boundaries) */
        uint16_t page_offset = addr % CAT24C512_PAGE_SIZE;
        uint16_t remaining_in_page = CAT24C512_PAGE_SIZE - page_offset;
        uint16_t chunk_size = (len < remaining_in_page) ? len : remaining_in_page;

        /* Prepare write buffer: [addr_hi, addr_lo, data...] */
        uint8_t buffer[CAT24C512_PAGE_SIZE + 2];
        buffer[0] = addr >> 8;
        buffer[1] = addr & 0xFF;
        memcpy(&buffer[2], data, chunk_size);

        /* Write chunk */
        int res = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, chunk_size + 2, false);

        if (res != (chunk_size + 2)) {
            ERROR_PRINT("[CAT24C512] WriteBuffer failed at 0x%04X (chunk %u bytes)\r\n", addr,
                        chunk_size);
            return -1;
        }

        write_cycle_delay();

        /* Move to next chunk */
        addr += chunk_size;
        data += chunk_size;
        len -= chunk_size;
    }

    return 0;
}

void CAT24C512_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len) {
    if (!buffer) {
        ERROR_PRINT("[CAT24C512] ReadBuffer: NULL buffer pointer\r\n");
        return;
    }

    uint8_t addr_buf[2] = {addr >> 8, addr & 0xFF};

    int res1 = i2c_write_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, addr_buf, 2, true);
    int res2 = i2c_read_blocking(EEPROM_I2C, CAT24C512_I2C_ADDR, buffer, len, false);

    if (res1 != 2 || res2 != (int)len) {
        ERROR_PRINT("[CAT24C512] ReadBuffer failed at 0x%04X (%lu bytes)\r\n", addr,
                    (unsigned long)len);
        /* Fill with 0xFF on failure */
        memset(buffer, 0xFF, len);
    }
}

void CAT24C512_Dump(uint8_t *buffer) {
    if (!buffer) {
        ERROR_PRINT("[CAT24C512] Dump: NULL buffer pointer\r\n");
        return;
    }

    INFO_PRINT("[CAT24C512] Dumping entire EEPROM (%u bytes)\r\n", CAT24C512_TOTAL_SIZE);
    CAT24C512_ReadBuffer(0x0000, buffer, CAT24C512_TOTAL_SIZE);
    INFO_PRINT("[CAT24C512] EEPROM dump complete\r\n");
}

bool CAT24C512_SelfTest(uint16_t test_addr) {
    const uint8_t test_pattern[] = {0xAA, 0x55, 0xCC, 0x33, 0xF0, 0x0F, 0x00, 0xFF};
    const uint8_t pattern_len = sizeof(test_pattern);
    uint8_t read_buffer[sizeof(test_pattern)];

    INFO_PRINT("[CAT24C512] Self-test starting at address 0x%04X\r\n", test_addr);

    /* Write test pattern */
    if (CAT24C512_WriteBuffer(test_addr, test_pattern, pattern_len) != 0) {
        ERROR_PRINT("[CAT24C512] Self-test FAILED: Write error\r\n");
        return false;
    }

    /* Small delay to ensure write completion */
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Read back test pattern */
    CAT24C512_ReadBuffer(test_addr, read_buffer, pattern_len);

    /* Compare */
    if (memcmp(test_pattern, read_buffer, pattern_len) != 0) {
        ERROR_PRINT("[CAT24C512] Self-test FAILED: Data mismatch\r\n");
        for (uint8_t i = 0; i < pattern_len; i++) {
            DEBUG_PRINT("Exp[%u]=%02X Read[%u]=%02X\r\n", i, test_pattern[i], i, read_buffer[i]);
        }
        return false;
    }

    INFO_PRINT("[CAT24C512] Self-test PASSED\r\n");
    return true;
}
