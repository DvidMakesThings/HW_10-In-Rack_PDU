/**
 * @file drivers/cat24c256_driver.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.0
 * @date 2025-11-06
 *
 * @details
 * Implementation of CAT24C256 32KB EEPROM driver with page-aware write logic.
 * All I2C operations use the centralized i2c_bus manager for thread safety.
 *
 * Write Strategy:
 * - Single bytes: Direct write + delay
 * - Multi-byte: Split into page-aligned chunks, delay after each chunk
 *
 * Read Strategy:
 * - Sequential reads with EEPROM auto-increment
 * - No page boundary restrictions
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define CAT24_TAG "[CAT24DRV]"

/**
 * @brief RTOS-compatible write cycle delay.
 *
 * @details
 * Implements the mandatory tWR (write cycle time) delay per CAT24C256 datasheet.
 * Uses RTOS task delay to allow other tasks to execute during wait period,
 * preventing watchdog starvation and improving system responsiveness.
 *
 * Timing Requirements:
 * - Typical: 5ms
 * - Maximum: 10ms
 * - This implementation: 5ms (CAT24C256_WRITE_CYCLE_MS)
 *
 * @param None
 * @return None
 *
 * @note Must be called after every write operation before next access
 * @note Uses vTaskDelay for cooperative multitasking
 */
static inline void write_cycle_delay(void) { vTaskDelay(pdMS_TO_TICKS(CAT24C256_WRITE_CYCLE_MS)); }

void CAT24C256_Init(void) {
    gpio_set_function(I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA);
    gpio_pull_up(I2C1_SCL);

    INFO_PRINT("%s Driver initialized\r\n", CAT24_TAG);
}

int CAT24C256_WriteByte(uint16_t addr, uint8_t data) {
    bool ok = i2c_bus_write_mem16(EEPROM_I2C, CAT24C256_I2C_ADDR, addr, &data, 1, 50000);

    if (ok) {
        write_cycle_delay();
        return 0;
    } else {
#ifdef ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CAT24, 0x0);
        ERROR_PRINT_CODE(errorcode, "%s WriteByte failed at 0x%04X\r\n", CAT24_TAG, addr);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return -1;
    }
}

uint8_t CAT24C256_ReadByte(uint16_t addr) {
    uint8_t data = 0xFF;
    bool ok = i2c_bus_read_mem16(EEPROM_I2C, CAT24C256_I2C_ADDR, addr, &data, 1, 50000);

    if (!ok) {
#ifdef ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CAT24, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s ReadByte failed at 0x%04X\r\n", CAT24_TAG, addr);
        Storage_EnqueueErrorCode(errorcode);
#endif
    }

    return data;
}

int CAT24C256_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len) {
    if (!data) {
#ifdef ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CAT24, 0x2);
        ERROR_PRINT_CODE(errorcode, "%s WriteBuffer: NULL data pointer\r\n", CAT24_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return -1;
    }

    while (len > 0) {
        uint16_t page_offset = addr % CAT24C256_PAGE_SIZE;
        uint16_t remaining_in_page = CAT24C256_PAGE_SIZE - page_offset;
        uint16_t chunk_size = (len < remaining_in_page) ? len : remaining_in_page;

        bool ok =
            i2c_bus_write_mem16(EEPROM_I2C, CAT24C256_I2C_ADDR, addr, data, chunk_size, 50000);

        if (!ok) {
#ifdef ERRORLOGGER
            uint16_t errorcode =
                ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CAT24, 0x3);
            ERROR_PRINT_CODE(errorcode, "%s WriteBuffer failed at 0x%04X (chunk %u bytes)\r\n",
                             CAT24_TAG, addr, chunk_size);
            Storage_EnqueueErrorCode(errorcode);
#endif
            return -1;
        }

        write_cycle_delay();

        addr += chunk_size;
        data += chunk_size;
        len -= chunk_size;
    }

    return 0;
}

void CAT24C256_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len) {
    if (!buffer) {
#ifdef ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CAT24, 0x4);
        ERROR_PRINT_CODE(errorcode, "%s ReadBuffer: NULL buffer pointer\r\n", CAT24_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return;
    }

    bool ok = i2c_bus_read_mem16(EEPROM_I2C, CAT24C256_I2C_ADDR, addr, buffer, len, 50000);

    if (!ok) {
#ifdef ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CAT24, 0x5);
        ERROR_PRINT_CODE(errorcode, "%s ReadBuffer failed at 0x%04X (%lu bytes)\r\n", CAT24_TAG,
                         addr, (unsigned long)len);
        Storage_EnqueueErrorCode(errorcode);
#endif
        memset(buffer, 0xFF, len);
    }
}

bool CAT24C256_SelfTest(uint16_t test_addr) {
    const uint8_t test_pattern[] = {0xAA, 0x55, 0xCC, 0x33, 0xF0, 0x0F, 0x00, 0xFF};
    const uint8_t pattern_len = sizeof(test_pattern);
    uint8_t read_buffer[sizeof(test_pattern)];

    INFO_PRINT("[CAT24C256] Self-test starting at address 0x%04X\r\n", test_addr);

    if (CAT24C256_WriteBuffer(test_addr, test_pattern, pattern_len) != 0) {
#ifdef ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CAT24, 0x6);
        ERROR_PRINT_CODE(errorcode, "%s Self-test WriteBuffer failed\r\n", CAT24_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    CAT24C256_ReadBuffer(test_addr, read_buffer, pattern_len);

    if (memcmp(test_pattern, read_buffer, pattern_len) != 0) {
#ifdef ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CAT24, 0x7);
        ERROR_PRINT_CODE(errorcode, "%s Self-test failed: Data mismatch\r\n", CAT24_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        for (uint8_t i = 0; i < pattern_len; i++) {
            DEBUG_PRINT("Exp[%u]=%02X Read[%u]=%02X\r\n", i, test_pattern[i], i, read_buffer[i]);
        }
        return false;
    }

    INFO_PRINT("[CAT24C256] Self-test PASSED\r\n");
    return true;
}
