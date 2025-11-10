/**
 * @file CAT24C512_driver.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers03 3. CAT24C512 EEPROM Driver Implementation
 * @ingroup drivers
 * @brief CAT24C512 I2C EEPROM driver implementation (RTOS-compatible)
 * @{
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

/**
 * @brief Initializes the CAT24C512 EEPROM hardware interface
 *
 * Configures the I2C GPIO pins for proper communication with the EEPROM device.
 * The I2C peripheral must be previously initialized by system_startup_init().
 *
 * @param None
 * @return None
 */
void CAT24C512_Init(void) {
    /* I2C peripheral already initialized by system_startup_init() */
    /* Just set GPIO function and pull-ups using CONFIG.h definitions */

    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);

    INFO_PRINT("[CAT24C512] Driver initialized (I2C1: SDA=%u, SCL=%u)\r\n", I2C1_SDA, I2C1_SCL);
}

/**
 * @brief Writes a single byte to the specified EEPROM address
 *
 * Creates a 3-byte I2C transaction containing the high byte of address,
 * low byte of address, and the data byte. Includes mandatory write cycle delay.
 *
 * @param addr Target memory address (0x0000 - 0xFFFF)
 * @param data Single byte to be written
 * @return 0 if write successful and acknowledged, -1 on any I2C error
 */
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

/**
 * @brief Reads a single byte from the specified EEPROM address
 *
 * Performs a two-part I2C transaction:
 * 1. Write address bytes with repeated start
 * 2. Read one byte of data
 *
 * @param addr Source memory address (0x0000 - 0xFFFF)
 * @return Read byte value, 0xFF if read operation fails
 */
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

/**
 * @brief Writes a contiguous block of data to EEPROM with page boundary handling
 *
 * Implements automatic page-aware write operations that respect the 128-byte
 * page boundaries of the CAT24C512. Write operations that cross page boundaries
 * are automatically split into multiple page-aligned chunks.
 *
 * @param addr Starting memory address (0x0000 - 0xFFFF)
 * @param data Pointer to source data buffer
 * @param len Number of bytes to write
 * @return 0 if all writes successful, -1 on NULL pointer or any I2C error
 * @note Includes write cycle delays between each page write operation
 */
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

/**
 * @brief Reads a contiguous block of data from EEPROM
 *
 * Performs a sequential read operation starting at the specified address.
 * Uses the EEPROM's auto-increment feature for efficient multi-byte reads.
 * On error, fills the destination buffer with 0xFF.
 *
 * @param addr Starting memory address (0x0000 - 0xFFFF)
 * @param buffer Pointer to destination buffer
 * @param len Number of bytes to read
 * @return None
 * @note Supports reads across page boundaries without special handling
 */
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

/**
 * @brief Retrieves entire EEPROM contents in a single operation
 *
 * Performs a complete memory dump of the CAT24C512, reading all 64KB
 * into the provided buffer. Useful for backup operations or debugging.
 *
 * @param buffer Pointer to pre-allocated buffer of at least 64KB (CAT24C512_TOTAL_SIZE)
 * @return None
 * @note Caller must ensure buffer is large enough to hold entire EEPROM contents
 */
void CAT24C512_Dump(uint8_t *buffer) {
    if (!buffer) {
        ERROR_PRINT("[CAT24C512] Dump: NULL buffer pointer\r\n");
        return;
    }

    INFO_PRINT("[CAT24C512] Dumping entire EEPROM (%u bytes)\r\n", CAT24C512_TOTAL_SIZE);
    CAT24C512_ReadBuffer(0x0000, buffer, CAT24C512_TOTAL_SIZE);
    INFO_PRINT("[CAT24C512] EEPROM dump complete\r\n");
}

void CAT24C512_DumpFormatted(void) {
    char line[256], field[16];
    TaskHandle_t h = xTaskGetCurrentTaskHandle();
    UBaseType_t old_prio = uxTaskPriorityGet(h);

    vTaskPrioritySet(h, configMAX_PRIORITIES - 1);

    log_printf("EE_DUMP_START\r\n");

    snprintf(line, sizeof(line), "%-7s", "Addr");
    for (uint8_t col = 0; col < 16; col++) {
        snprintf(field, sizeof(field), "%-7X", col);
        strncat(line, field, sizeof(line) - strlen(line) - 1);
    }
    log_printf("%s\r\n", line);

    for (uint32_t addr = 0; addr < CAT24C512_TOTAL_SIZE; addr += 16) {
        uint8_t buffer[16];
        CAT24C512_ReadBuffer((uint16_t)addr, buffer, 16);
        snprintf(line, sizeof(line), "0x%04X ", (uint16_t)addr);
        for (uint8_t i = 0; i < 16; i++) {
            snprintf(field, sizeof(field), "%02X ", buffer[i]);
            strncat(line, field, sizeof(line) - strlen(line) - 1);
        }
        log_printf("%s\r\n", line);
        fflush(stdout);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    log_printf("EE_DUMP_END\r\n");

    vTaskPrioritySet(h, old_prio);
}

/**
 * @brief Performs a comprehensive self-test of EEPROM functionality
 *
 * Validates EEPROM operation by:
 * 1. Writing a test pattern with alternating bits and edge cases
 * 2. Reading back the pattern to verify data integrity
 * 3. Comparing written and read data byte-by-byte
 *
 * Test pattern: 0xAA, 0x55, 0xCC, 0x33, 0xF0, 0x0F, 0x00, 0xFF
 * - Tests alternating bits (0xAA, 0x55)
 * - Tests paired bits (0xCC, 0x33)
 * - Tests half-byte patterns (0xF0, 0x0F)
 * - Tests extreme values (0x00, 0xFF)
 *
 * @param test_addr Starting address for test pattern
 * @return true if test passes, false on any error
 * @note Modifies 8 bytes of EEPROM content starting at test_addr
 */
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

/**
 * @}
 */