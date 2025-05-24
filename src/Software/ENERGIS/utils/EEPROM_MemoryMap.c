/**
 * @file EEPROM_MemoryMap.c
 * @author DvidMakesThings - David Sipos
 * @brief Implementation of the EEPROM Memory Map functions.
 * @version 1.4
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "utils/EEPROM_MemoryMap.h"
#include "drivers/CAT24C512_driver.h"
#include <stdio.h>
#include <string.h>

// Size of record: struct + 1‐byte CRC
#define USER_PREF_RECORD_SIZE (sizeof(userPrefInfo) + 1)

static uint8_t calculate_crc8(const uint8_t *data, size_t len);

/**
 * @brief Factory defaults (only in this .c, not exposed as extern).
 */
static const userPrefInfo DEFAULT_USER_PREFS = {
    .device_name = "ENERGIS", .location = "Server Room", .temp_unit = 0};

// ======================================================================
//                     SYSTEM INFO & CALIBRATION
// ======================================================================

/**
 * @brief Writes system info to EEPROM.
 * @param data Pointer to system info data.
 * @param len Data length (must be <= EEPROM_SYS_INFO_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_SYS_INFO_START, data, len);
}

/**
 * @brief Reads system info from EEPROM.
 * @param data Pointer to buffer to store system info.
 * @param len Data length (must be <= EEPROM_SYS_INFO_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_SYS_INFO_START, data, len);
    return 0;
}

// ======================================================================
//                     FACTORY DEFAULT SETTINGS
// ======================================================================

/**
 * @brief Writes factory default settings to EEPROM.
 *
 * This function resets all EEPROM sections to their default values.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteFactoryDefaults(void) {
    int status = 0;

    // Write default Serial Number
    status |= EEPROM_WriteSystemInfo((const uint8_t *)DEFAULT_SN, strlen(DEFAULT_SN) + 1);
    INFO_PRINT("Serial Number written to EEPROM.\n");

    // Write default Relay Status (All OFF)
    status |= EEPROM_WriteUserOutput(DEFAULT_RELAY_STATUS, sizeof(DEFAULT_RELAY_STATUS));
    INFO_PRINT("Relay Status written to EEPROM.\n");

    // Write default Network Configuration
    status |= EEPROM_WriteUserNetworkWithChecksum(&DEFAULT_NETWORK);
    INFO_PRINT("Network Configuration written to EEPROM.\n");

    // Write default Calibration Data
    status |= EEPROM_WriteSensorCalibration(DEFAULT_CALIB_DATA, sizeof(DEFAULT_CALIB_DATA));
    INFO_PRINT("Sensor Calibration written to EEPROM.\n");

    // Write default Energy Monitoring Data
    status |= EEPROM_WriteEnergyMonitoring(DEFAULT_ENERGY_DATA, sizeof(DEFAULT_ENERGY_DATA));
    INFO_PRINT("Energy Monitoring Data written to EEPROM.\n");

    // Write default Log Data
    status |= EEPROM_WriteEventLogs(DEFAULT_LOG_DATA, sizeof(DEFAULT_LOG_DATA));
    INFO_PRINT("Event Logs written to EEPROM.\n");

    // Write default User Preferences
    status |= EEPROM_WriteUserPreferences(DEFAULT_USER_PREF, sizeof(DEFAULT_USER_PREF));
    INFO_PRINT("User Preferences written to EEPROM.\n");

    if (status == 0) {
        INFO_PRINT("EEPROM factory defaults written successfully.\n");
    } else {
        INFO_PRINT("EEPROM factory defaults write encountered errors.\n");
    }
    return status;
}

/**
 * @brief Reads and validates factory defaults stored in EEPROM.
 * If any section is invalid, it restores the defaults.
 * @return 0 if all defaults are valid, -1 if defaults were restored.
 */
int EEPROM_ReadFactoryDefaults(void) {
    int needs_reset = 0;

    // Validate Serial Number
    char stored_sn[strlen(DEFAULT_SN) + 1];
    EEPROM_ReadSystemInfo((uint8_t *)stored_sn, strlen(DEFAULT_SN) + 1);
    if (memcmp(stored_sn, DEFAULT_SN, strlen(DEFAULT_SN)) != 0) {
        ERROR_PRINT("Invalid Serial Number detected in EEPROM.\n");
        needs_reset = 1;
    }

    // Validate Relay Status
    uint8_t stored_relay_status[sizeof(DEFAULT_RELAY_STATUS)];
    EEPROM_ReadUserOutput(stored_relay_status, sizeof(DEFAULT_RELAY_STATUS));
    if (memcmp(stored_relay_status, DEFAULT_RELAY_STATUS, sizeof(DEFAULT_RELAY_STATUS)) != 0) {
        ERROR_PRINT("Invalid Relay Status detected in EEPROM.\n");
        needs_reset = 1;
    }

    // Validate Network Configuration
    networkInfo stored_network;
    EEPROM_ReadUserNetworkWithChecksum(&stored_network);
    if (memcmp(&stored_network, &DEFAULT_NETWORK, sizeof(networkInfo)) != 0) {
        ERROR_PRINT("Invalid Network Configuration detected in EEPROM.\n");
        needs_reset = 1;
    }

    // Validate Sensor Calibration
    uint8_t stored_calib[sizeof(DEFAULT_CALIB_DATA)];
    EEPROM_ReadSensorCalibration(stored_calib, sizeof(DEFAULT_CALIB_DATA));
    if (memcmp(stored_calib, DEFAULT_CALIB_DATA, sizeof(DEFAULT_CALIB_DATA)) != 0) {
        ERROR_PRINT("Invalid Sensor Calibration detected in EEPROM.\n");
        needs_reset = 1;
    }

    // Validate Energy Monitoring Data
    uint8_t stored_energy[sizeof(DEFAULT_ENERGY_DATA)];
    EEPROM_ReadEnergyMonitoring(stored_energy, sizeof(DEFAULT_ENERGY_DATA));
    if (memcmp(stored_energy, DEFAULT_ENERGY_DATA, sizeof(DEFAULT_ENERGY_DATA)) != 0) {
        ERROR_PRINT("Invalid Energy Monitoring Data detected in EEPROM.\n");
        needs_reset = 1;
    }

    // Validate Event Logs
    uint8_t stored_log[sizeof(DEFAULT_LOG_DATA)];
    EEPROM_ReadEventLogs(stored_log, sizeof(DEFAULT_LOG_DATA));
    if (memcmp(stored_log, DEFAULT_LOG_DATA, sizeof(DEFAULT_LOG_DATA)) != 0) {
        ERROR_PRINT("Invalid Event Logs detected in EEPROM.\n");
        needs_reset = 1;
    }

    // Validate User Preferences
    uint8_t stored_user_pref[sizeof(DEFAULT_USER_PREF)];
    EEPROM_ReadUserPreferences(stored_user_pref, sizeof(DEFAULT_USER_PREF));
    if (memcmp(stored_user_pref, DEFAULT_USER_PREF, sizeof(DEFAULT_USER_PREF)) != 0) {
        ERROR_PRINT("Invalid User Preferences detected in EEPROM.\n");
        needs_reset = 1;
    }

    // If any section is invalid, restore factory defaults
    if (needs_reset) {
        ERROR_PRINT("EEPROM factory defaults are corrupted. Restoring defaults.\n");
        EEPROM_WriteFactoryDefaults();
        DEBUG_PRINT("DEBUG: Factory defaults restored, re-reading EEPROM.\n");

        // Re-run the function to confirm EEPROM is now correct
        return EEPROM_ReadFactoryDefaults();
    }

    INFO_PRINT("EEPROM content is correct.\n\n");
    return 0;
}

// ======================================================================
//                     USER OUTPUT CONFIGURATION
// ======================================================================

/**
 * @brief Writes user output configuration to EEPROM.
 * @param data Pointer to user output data.
 * @param len Data length (must be <= EEPROM_USER_OUTPUT_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_OUTPUT_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_OUTPUT_START, data, len);
}

/**
 * @brief Reads user output configuration from EEPROM.
 * @param data Pointer to buffer to store output configuration.
 * @param len Data length (must be <= EEPROM_USER_OUTPUT_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadUserOutput(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_OUTPUT_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_OUTPUT_START, data, len);
    return 0;
}

// ======================================================================
//                     USER NETWORK SETTINGS
// ======================================================================

/**
 * @brief Writes user network settings to EEPROM.
 * @param data Pointer to network settings data.
 * @param len Data length (must be <= EEPROM_USER_NETWORK_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_NETWORK_START, data, len);
}

/**
 * @brief Reads user network settings from EEPROM.
 * @param data Pointer to buffer to store network settings.
 * @param len Data length (must be <= EEPROM_USER_NETWORK_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_NETWORK_START, data, len);
    return 0;
}

// ======================================================================
//                     SENSOR CALIBRATION
// ======================================================================

/**
 * @brief Writes sensor calibration data to EEPROM.
 * @param data Pointer to calibration data.
 * @param len Data length (must be <= EEPROM_SENSOR_CAL_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_SENSOR_CAL_START, data, len);
}

/**
 * @brief Reads sensor calibration data from EEPROM.
 * @param data Pointer to buffer to store calibration data.
 * @param len Data length (must be <= EEPROM_SENSOR_CAL_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_SENSOR_CAL_START, data, len);
    return 0;
}

// ======================================================================
//                     ENERGY MONITORING DATA
// ======================================================================

/**
 * @brief Writes energy monitoring data to EEPROM.
 * @param data Pointer to energy data.
 * @param len Data length (must be <= EEPROM_ENERGY_MON_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START, data, len);
}

/**
 * @brief Reads energy monitoring data from EEPROM.
 * @param data Pointer to buffer to store energy data.
 * @param len Data length (must be <= EEPROM_ENERGY_MON_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_ENERGY_MON_START, data, len);
    return 0;
}

// ======================================================================
//                     EVENT LOGS & FAULT HISTORY
// ======================================================================

/**
 * @brief Writes event log data to EEPROM.
 * @param data Pointer to event log data.
 * @param len Data length (must be <= EEPROM_EVENT_LOG_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START, data, len);
}

/**
 * @brief Reads event log data from EEPROM.
 * @param data Pointer to buffer to store event log data.
 * @param len Data length (must be <= EEPROM_EVENT_LOG_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_EVENT_LOG_START, data, len);
    return 0;
}

// ======================================================================
//                     USER PREFERENCES
// ======================================================================

/**
 * @brief Writes user preferences to EEPROM.
 * @param data Pointer to user preferences data.
 * @param len Data length (must be <= EEPROM_USER_PREF_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, data, len);
}

/**
 * @brief Reads user preferences from EEPROM.
 * @param data Pointer to buffer to store user preferences.
 * @param len Data length (must be <= EEPROM_USER_PREF_SIZE).
 * @return 0 on success, -1 on error.
 */
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, data, len);
    return 0;
}

// ======================================================================
//                     DATA INTEGRITY & CHECKSUM
// ======================================================================

/**
 * @brief Calculates CRC-8 checksum for given data.
 * @param data Pointer to data buffer.
 * @param len Length of data buffer.
 * @return Computed CRC-8 checksum.
 */
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

/**
 * @brief Writes system info with CRC-8 checksum.
 * @param data Pointer to system info data.
 * @param len Data length (must be <= EEPROM_SYS_INFO_SIZE - 1).
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1)
        return -1; // Reserve 1 byte for CRC
    uint8_t buffer[len + 1];
    memcpy(buffer, data, len);
    buffer[len] = calculate_crc8(data, len);
    return CAT24C512_WriteBuffer(EEPROM_SYS_INFO_START, buffer, len + 1);
}

/**
 * @brief Reads system info with CRC-8 checksum verification.
 * @param data Pointer to buffer to store system info.
 * @param len Data length (must be <= EEPROM_SYS_INFO_SIZE - 1).
 * @return 0 if checksum is valid, -1 if invalid.
 */
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1)
        return -1;
    uint8_t buffer[len + 1];
    CAT24C512_ReadBuffer(EEPROM_SYS_INFO_START, buffer, len + 1);
    uint8_t crc = calculate_crc8(buffer, len);
    if (crc != buffer[len]) {
        return -1; // CRC mismatch
    }
    memcpy(data, buffer, len);
    return 0;
}

/**
 * @brief Writes user network settings with CRC-8 checksum.
 * @param net_info Pointer to networkInfo struct.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info) {
    uint8_t buffer[24]; // 16 (IP, GW, SN, MAC) + 4 (DNS) + 1 (DHCP) + 1 (Checksum)

    // Copy data into buffer (order: IP, GW, SN, MAC, DNS, DHCP)
    memcpy(&buffer[0], net_info->ip, 4);
    memcpy(&buffer[4], net_info->gw, 4);
    memcpy(&buffer[8], net_info->sn, 4);
    memcpy(&buffer[12], net_info->mac, 6);
    memcpy(&buffer[18], net_info->dns, 4);
    buffer[22] = net_info->dhcp; // Store DHCP mode

    // Calculate and append CRC-8 checksum
    buffer[23] = calculate_crc8(buffer, 23);

    return CAT24C512_WriteBuffer(EEPROM_USER_NETWORK_START, buffer, 24);
}

/**
 * @brief Reads user network settings with CRC-8 checksum verification.
 * @param net_info Pointer to buffer to store network configuration.
 * @return 0 if checksum is valid, -1 if invalid.
 */
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info) {
    uint8_t buffer[24]; // 16 + 4 + 1 + 1

    CAT24C512_ReadBuffer(EEPROM_USER_NETWORK_START, buffer, 24);

    // Validate CRC-8
    if (calculate_crc8(buffer, 23) == buffer[23]) {
        memcpy(net_info->ip, &buffer[0], 4);
        memcpy(net_info->gw, &buffer[4], 4);
        memcpy(net_info->sn, &buffer[8], 4);
        memcpy(net_info->mac, &buffer[12], 6);
        memcpy(net_info->dns, &buffer[18], 4);
        net_info->dhcp = buffer[22]; // Restore DHCP mode
        return 0;                    // Valid data
    }
    return -1; // Checksum error
}

// ======================================================================
//                     WEAR LEVELING & APPEND
// ======================================================================

/**
 * @brief Reads the current write pointer for energy monitoring records.
 * @return Current write pointer position.
 */
static uint16_t read_energy_pointer(void) {
    uint16_t ptr = ((uint16_t)CAT24C512_ReadByte(EEPROM_ENERGY_MON_START) << 8) |
                   CAT24C512_ReadByte(EEPROM_ENERGY_MON_START + 1);
    return ptr;
}

/**
 * @brief Updates the write pointer for energy monitoring records.
 * @param ptr New pointer position.
 */
static void write_energy_pointer(uint16_t ptr) {
    CAT24C512_WriteByte(EEPROM_ENERGY_MON_START, ptr >> 8);
    CAT24C512_WriteByte(EEPROM_ENERGY_MON_START + 1, ptr & 0xFF);
}

/**
 * @brief Appends an energy monitoring record to EEPROM.
 * @param data Pointer to energy record (must be ENERGY_RECORD_SIZE bytes).
 * @return 0 on success, -1 on error.
 */
int EEPROM_AppendEnergyRecord(const uint8_t *data) {
    uint16_t ptr = read_energy_pointer();
    uint16_t effective_size = EEPROM_ENERGY_MON_SIZE - ENERGY_MON_POINTER_SIZE;
    if (ptr + ENERGY_RECORD_SIZE > effective_size) {
        ptr = 0; // Wrap around if necessary
    }
    int res = CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START + ENERGY_MON_POINTER_SIZE + ptr, data,
                                    ENERGY_RECORD_SIZE);
    if (res != 0)
        return res;
    ptr += ENERGY_RECORD_SIZE;
    write_energy_pointer(ptr);
    return 0;
}

/**
 * @brief Reads the current write pointer for event logs.
 * @return Current write pointer position.
 */
static uint16_t read_event_log_pointer(void) {
    uint16_t ptr = ((uint16_t)CAT24C512_ReadByte(EEPROM_EVENT_LOG_START) << 8) |
                   CAT24C512_ReadByte(EEPROM_EVENT_LOG_START + 1);
    return ptr;
}

/**
 * @brief Updates the write pointer for event logs.
 * @param ptr New pointer position.
 */
static void write_event_log_pointer(uint16_t ptr) {
    CAT24C512_WriteByte(EEPROM_EVENT_LOG_START, ptr >> 8);
    CAT24C512_WriteByte(EEPROM_EVENT_LOG_START + 1, ptr & 0xFF);
}

/**
 * @brief Appends an event log entry to EEPROM.
 * @param entry Pointer to log entry (must be EVENT_LOG_ENTRY_SIZE bytes).
 * @return 0 on success, -1 on error.
 */
int EEPROM_AppendEventLog(const uint8_t *entry) {
    uint16_t ptr = read_event_log_pointer();
    uint16_t effective_size = EEPROM_EVENT_LOG_SIZE - EVENT_LOG_POINTER_SIZE;
    if (ptr + EVENT_LOG_ENTRY_SIZE > effective_size) {
        ptr = 0; // Wrap around if necessary
    }
    int res = CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START + EVENT_LOG_POINTER_SIZE + ptr, entry,
                                    EVENT_LOG_ENTRY_SIZE);
    if (res != 0)
        return res;
    ptr += EVENT_LOG_ENTRY_SIZE;
    write_event_log_pointer(ptr);
    return 0;
}

int EEPROM_WriteUserPrefsWithChecksum(const userPrefInfo *prefs) {
    uint8_t buf[USER_PREF_RECORD_SIZE];
    memcpy(buf, prefs, sizeof(userPrefInfo));
    buf[sizeof(userPrefInfo)] = calculate_crc8(buf, sizeof(userPrefInfo));
    return CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, buf, USER_PREF_RECORD_SIZE);
}

int EEPROM_ReadUserPrefsWithChecksum(userPrefInfo *prefs) {
    uint8_t buf[USER_PREF_RECORD_SIZE];
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, buf, USER_PREF_RECORD_SIZE);
    if (calculate_crc8(buf, sizeof(userPrefInfo)) != buf[sizeof(userPrefInfo)])
        return -1;
    memcpy(prefs, buf, sizeof(userPrefInfo));
    return 0;
}

userPrefInfo LoadUserPreferences(void) {
    userPrefInfo prefs;
    if (EEPROM_ReadUserPrefsWithChecksum(&prefs) == 0) {
        INFO_PRINT("Loaded user prefs\n");
    } else {
        ERROR_PRINT("No saved prefs, using defaults\n");
        prefs = DEFAULT_USER_PREFS;
    }
    return prefs;
}

networkInfo LoadUserNetworkConfig(void) {
    networkInfo net_info;
    if (EEPROM_ReadUserNetworkWithChecksum(&net_info) == 0) {
        INFO_PRINT("Loaded network config from EEPROM.\n");
    } else {
        ERROR_PRINT("EEPROM network config invalid. Using defaults.\n");
        net_info = DEFAULT_NETWORK;
    }
    return net_info;
}