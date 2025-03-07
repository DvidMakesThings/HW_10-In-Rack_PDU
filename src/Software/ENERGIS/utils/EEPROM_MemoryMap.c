/**
 * @file EEPROM_MemoryMap.c
 * @author David Sipos
 * @brief Implementation of the EEPROM Memory Map functions.
 * @version 1.4
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "utils/EEPROM_MemoryMap.h"
#include "drivers/CAT24C512_driver.h"  // Uses your EEPROM driver functions
#include <string.h>

//----------------------------
// Existing region implementations
// 1. System Info & Calibration
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE) return -1;
    return CAT24C512_WriteBuffer(EEPROM_SYS_INFO_START, data, len);
}

int EEPROM_ReadSystemInfo(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE) return -1;
    CAT24C512_ReadBuffer(EEPROM_SYS_INFO_START, data, len);
    return 0;
}

// 2. Factory Default Settings
int EEPROM_WriteFactoryDefaults(const uint8_t *data, size_t len) {
    if (len > EEPROM_FACTORY_DEFAULTS_SIZE) return -1;
    return CAT24C512_WriteBuffer(EEPROM_FACTORY_DEFAULTS_START, data, len);
}

int EEPROM_ReadFactoryDefaults(uint8_t *data, size_t len) {
    if (len > EEPROM_FACTORY_DEFAULTS_SIZE) return -1;
    CAT24C512_ReadBuffer(EEPROM_FACTORY_DEFAULTS_START, data, len);
    return 0;
}

// 3. User Output Configuration
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_OUTPUT_SIZE) return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_OUTPUT_START, data, len);
}

int EEPROM_ReadUserOutput(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_OUTPUT_SIZE) return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_OUTPUT_START, data, len);
    return 0;
}

// 4. User Network Settings
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE) return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_NETWORK_START, data, len);
}

int EEPROM_ReadUserNetwork(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE) return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_NETWORK_START, data, len);
    return 0;
}

// 5. Sensor Calibration & Thresholds
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE) return -1;
    return CAT24C512_WriteBuffer(EEPROM_SENSOR_CAL_START, data, len);
}

int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE) return -1;
    CAT24C512_ReadBuffer(EEPROM_SENSOR_CAL_START, data, len);
    return 0;
}

// 6. Energy Monitoring Data (Basic read/write functions)
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE) return -1;
    return CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START, data, len);
}

int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE) return -1;
    CAT24C512_ReadBuffer(EEPROM_ENERGY_MON_START, data, len);
    return 0;
}

// 7. Event Logs & Fault History (Basic read/write functions)
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE) return -1;
    return CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START, data, len);
}

int EEPROM_ReadEventLogs(uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE) return -1;
    CAT24C512_ReadBuffer(EEPROM_EVENT_LOG_START, data, len);
    return 0;
}

// 8. User Preferences
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE) return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, data, len);
}

int EEPROM_ReadUserPreferences(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE) return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, data, len);
    return 0;
}

//----------------------------
// New Code for Data Integrity (Checksum)

// CRC-8 implementation
static uint8_t calculate_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// System Info with Checksum: Data length must be <= EEPROM_SYS_INFO_SIZE - 1
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1) return -1; // reserve 1 byte for CRC
    uint8_t buffer[len + 1];
    memcpy(buffer, data, len);
    buffer[len] = calculate_crc8(data, len);
    return CAT24C512_WriteBuffer(EEPROM_SYS_INFO_START, buffer, len + 1);
}

int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1) return -1;
    uint8_t buffer[len + 1];
    CAT24C512_ReadBuffer(EEPROM_SYS_INFO_START, buffer, len + 1);
    uint8_t crc = calculate_crc8(buffer, len);
    if (crc != buffer[len]) {
        return -1;  // CRC mismatch
    }
    memcpy(data, buffer, len);
    return 0;
}

// User Network with Checksum: Data length must be <= EEPROM_USER_NETWORK_SIZE - 1
int EEPROM_WriteUserNetworkWithChecksum(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE - 1) return -1;
    uint8_t buffer[len + 1];
    memcpy(buffer, data, len);
    buffer[len] = calculate_crc8(data, len);
    return CAT24C512_WriteBuffer(EEPROM_USER_NETWORK_START, buffer, len + 1);
}

int EEPROM_ReadUserNetworkWithChecksum(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE - 1) return -1;
    uint8_t buffer[len + 1];
    CAT24C512_ReadBuffer(EEPROM_USER_NETWORK_START, buffer, len + 1);
    uint8_t crc = calculate_crc8(buffer, len);
    if (crc != buffer[len]) {
        return -1;  // CRC mismatch
    }
    memcpy(data, buffer, len);
    return 0;
}

//----------------------------
// New Code for Wear Leveling / Appending Records

// Energy Monitoring: Use first 2 bytes as pointer.
static uint16_t read_energy_pointer(void) {
    uint16_t ptr = ((uint16_t)CAT24C512_ReadByte(EEPROM_ENERGY_MON_START) << 8)
                   | CAT24C512_ReadByte(EEPROM_ENERGY_MON_START + 1);
    return ptr;
}

static void write_energy_pointer(uint16_t ptr) {
    CAT24C512_WriteByte(EEPROM_ENERGY_MON_START, ptr >> 8);
    CAT24C512_WriteByte(EEPROM_ENERGY_MON_START + 1, ptr & 0xFF);
}

// Append a fixed-size energy record (ENERGY_RECORD_SIZE bytes)
int EEPROM_AppendEnergyRecord(const uint8_t *data) {
    uint16_t ptr = read_energy_pointer();
    uint16_t effective_size = EEPROM_ENERGY_MON_SIZE - ENERGY_MON_POINTER_SIZE;
    if (ptr + ENERGY_RECORD_SIZE > effective_size) {
        ptr = 0;  // Wrap around if necessary
    }
    int res = CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START + ENERGY_MON_POINTER_SIZE + ptr, data, ENERGY_RECORD_SIZE);
    if (res != 0) return res;
    ptr += ENERGY_RECORD_SIZE;
    write_energy_pointer(ptr);
    return 0;
}

// Event Logs: Use first 2 bytes as pointer.
static uint16_t read_event_log_pointer(void) {
    uint16_t ptr = ((uint16_t)CAT24C512_ReadByte(EEPROM_EVENT_LOG_START) << 8)
                   | CAT24C512_ReadByte(EEPROM_EVENT_LOG_START + 1);
    return ptr;
}

static void write_event_log_pointer(uint16_t ptr) {
    CAT24C512_WriteByte(EEPROM_EVENT_LOG_START, ptr >> 8);
    CAT24C512_WriteByte(EEPROM_EVENT_LOG_START + 1, ptr & 0xFF);
}

// Append a fixed-size event log entry (EVENT_LOG_ENTRY_SIZE bytes)
int EEPROM_AppendEventLog(const uint8_t *entry) {
    uint16_t ptr = read_event_log_pointer();
    uint16_t effective_size = EEPROM_EVENT_LOG_SIZE - EVENT_LOG_POINTER_SIZE;
    if (ptr + EVENT_LOG_ENTRY_SIZE > effective_size) {
        ptr = 0;  // Wrap around if necessary
    }
    int res = CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START + EVENT_LOG_POINTER_SIZE + ptr, entry, EVENT_LOG_ENTRY_SIZE);
    if (res != 0) return res;
    ptr += EVENT_LOG_ENTRY_SIZE;
    write_event_log_pointer(ptr);
    return 0;
}
