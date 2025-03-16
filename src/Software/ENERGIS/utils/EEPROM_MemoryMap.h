/**
 * @file EEPROM_MemoryMap.h
 * @author David Sipos
 * @brief Header file for the EEPROM Memory Map and functions.
 * @version 1.4
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef EEPROM_MEMORY_MAP_H
#define EEPROM_MEMORY_MAP_H

#include <stddef.h>
#include <stdint.h>

/** @brief Total EEPROM size (64KB) */
#define EEPROM_SIZE 65536

// ======================================================================
//                          EEPROM MEMORY MAP
// ======================================================================

/**
 * @brief System Info & Calibration (256 bytes)
 *        - Stores firmware version and device serial number.
 *        - Last byte is reserved for an 8-bit checksum.
 */
#define EEPROM_SYS_INFO_START 0x0000
#define EEPROM_SYS_INFO_SIZE 0x0100

/**
 * @brief Factory Default Settings (256 bytes)
 *        - Stores factory preset configurations.
 */
#define EEPROM_FACTORY_DEFAULTS_START 0x0100
#define EEPROM_FACTORY_DEFAULTS_SIZE 0x0100

/**
 * @brief User Output Configuration (256 bytes)
 *        - Stores relay states, output configurations, etc.
 */
#define EEPROM_USER_OUTPUT_START 0x0200
#define EEPROM_USER_OUTPUT_SIZE 0x0100

/**
 * @brief User Network Settings (256 bytes)
 *        - Stores static/dynamic network configuration.
 *        - Last byte reserved for an 8-bit checksum.
 */
#define EEPROM_USER_NETWORK_START 0x0300
#define EEPROM_USER_NETWORK_SIZE (sizeof(networkInfo) + 1)

/**
 * @brief Sensor Calibration & Thresholds (1024 bytes)
 *        - Stores calibration data for sensors.
 */
#define EEPROM_SENSOR_CAL_START 0x0400
#define EEPROM_SENSOR_CAL_SIZE 0x0400

/**
 * @brief Energy Monitoring Data (2048 bytes)
 *        - Stores power consumption history.
 *        - First 2 bytes are reserved for a write pointer (wear leveling).
 */
#define EEPROM_ENERGY_MON_START 0x0800
#define EEPROM_ENERGY_MON_SIZE 0x0800
#define ENERGY_MON_POINTER_SIZE 2
#define ENERGY_RECORD_SIZE 16 /**< Fixed record size */

/**
 * @brief Event Logs & Fault History (4096 bytes)
 *        - Stores system logs and error history.
 *        - First 2 bytes are reserved for an append pointer.
 */
#define EEPROM_EVENT_LOG_START 0x1000
#define EEPROM_EVENT_LOG_SIZE 0x1000
#define EVENT_LOG_POINTER_SIZE 2
#define EVENT_LOG_ENTRY_SIZE 32 /**< Fixed entry size */

/**
 * @brief User Preferences (4096 bytes)
 *        - Stores user-configurable settings.
 */
#define EEPROM_USER_PREF_START 0x2000
#define EEPROM_USER_PREF_SIZE 0x1000

/**
 * @brief Reserved for Future Expansion (remaining EEPROM space).
 */
#define EEPROM_RESERVED_START 0x3000
#define EEPROM_RESERVED_SIZE (EEPROM_SIZE - EEPROM_RESERVED_START)

/** @brief Network Configuration Modes */
#define EEPROM_NETINFO_STATIC 0
#define EEPROM_NETINFO_DHCP 1

// ======================================================================
//                         STRUCT DEFINITIONS
// ======================================================================

/**
 * @struct networkInfo
 * @brief Stores network settings.
 */
typedef struct {
    uint8_t mac[6]; /**< MAC Address */
    uint8_t ip[4];  /**< IP Address */
    uint8_t sn[4];  /**< Subnet Mask */
    uint8_t gw[4];  /**< Gateway */
    uint8_t dns[4]; /**< DNS Server */
    uint8_t dhcp;   /**< DHCP Mode (0 = Static, 1 = DHCP) */
} networkInfo;

// ======================================================================
//                         DEFAULT VALUES
// ======================================================================

/**
 * @brief Default values for EEPROM initialization.
 */
#define DEFAULT_SN "SN-369366060325"

static const uint8_t DEFAULT_RELAY_STATUS[8] = {0}; /**< All relays OFF */

static const networkInfo DEFAULT_NETWORK = {
    .ip = {192, 168, 0, 11},                     // IP Address
    .gw = {192, 168, 0, 1},                      // Gateway
    .sn = {255, 255, 255, 0},                    // Subnet Mask
    .mac = {0x00, 0x08, 0xDC, 0xBE, 0xEF, 0x91}, // MAC Address
    .dns = {8, 8, 8, 8},                         // DNS Server
    .dhcp = EEPROM_NETINFO_STATIC                // Static IP Mode
};

static const uint8_t DEFAULT_CALIB_DATA[64] = {0};  /**< Default calibration data */
static const uint8_t DEFAULT_ENERGY_DATA[64] = {0}; /**< Default energy monitoring data */
static const uint8_t DEFAULT_LOG_DATA[64] = {0};    /**< Default event log data */
static const uint8_t DEFAULT_USER_PREF[64] = {0};   /**< Default user preferences */

// ======================================================================
//                      EEPROM READ/WRITE FUNCTIONS
// ======================================================================

/** @brief System Info */
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len);
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len);

/** @brief Factory Default Settings */
int EEPROM_WriteFactoryDefaults(void);
int EEPROM_ReadFactoryDefaults(void);

/** @brief User Output Configuration */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len);
int EEPROM_ReadUserOutput(uint8_t *data, size_t len);

/** @brief User Network Settings */
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len);
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len);

/** @brief Sensor Calibration */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len);
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len);

/** @brief Energy Monitoring Data */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len);
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len);

/** @brief Event Logs */
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len);
int EEPROM_ReadEventLogs(uint8_t *data, size_t len);

/** @brief User Preferences */
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len);
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len);

// ======================================================================
//               DATA INTEGRITY & CHECKSUM FUNCTIONS
// ======================================================================

/**
 * @brief Writes System Info with CRC Checksum.
 * @param data Pointer to system info data.
 * @param len Data length (must be <= EEPROM_SYS_INFO_SIZE - 1).
 * @return 0 if success, -1 if failure.
 */
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len);

/**
 * @brief Reads System Info with CRC Checksum.
 * @param data Pointer to store system info data.
 * @param len Data length (must be <= EEPROM_SYS_INFO_SIZE - 1).
 * @return 0 if checksum is valid, -1 if invalid.
 */
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len);

/**
 * @brief Writes User Network Config with CRC Checksum.
 * @param net_info Pointer to network configuration structure.
 * @return 0 if success, -1 if failure.
 */
int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info);

/**
 * @brief Reads User Network Config with CRC Checksum.
 * @param net_info Pointer to store network configuration.
 * @return 0 if checksum is valid, -1 if invalid.
 */
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info);

// ======================================================================
//               WEAR LEVELING & APPEND FUNCTIONS
// ======================================================================

/**
 * @brief Appends an energy monitoring record.
 * @param data Pointer to energy data (must be ENERGY_RECORD_SIZE bytes).
 * @return 0 if success, -1 if failure.
 */
int EEPROM_AppendEnergyRecord(const uint8_t *data);

/**
 * @brief Appends an event log entry.
 * @param entry Pointer to log entry (must be EVENT_LOG_ENTRY_SIZE bytes).
 * @return 0 if success, -1 if failure.
 */
int EEPROM_AppendEventLog(const uint8_t *entry);

/**
 * @brief Loads User Network Configuration.
 * @return The networkInfo structure containing the stored network configuration.
 */
networkInfo LoadUserNetworkConfig(void);

#endif // EEPROM_MEMORY_MAP_H
