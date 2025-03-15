/**
 * @file EEPROM_MemoryMap.h
 * @author David Sipos
 * @brief Header file for the the EEPROM Memory Map and functions.
 * @version 1.4
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef EEPROM_MEMORY_MAP_H
#define EEPROM_MEMORY_MAP_H

#include <stdint.h>
#include <stddef.h>

// Total EEPROM size (64KB)
#define EEPROM_SIZE               65536

// Memory Map Regions
// 1. System Info & Calibration (256 bytes)
//    Last byte is reserved for an 8-bit checksum.
#define EEPROM_SYS_INFO_START     0x0000
#define EEPROM_SYS_INFO_SIZE      0x0100

// 2. Factory Default Settings (256 bytes)
#define EEPROM_FACTORY_DEFAULTS_START  0x0100
#define EEPROM_FACTORY_DEFAULTS_SIZE   0x0100

// 3. User Output Configuration (256 bytes)
#define EEPROM_USER_OUTPUT_START  0x0200
#define EEPROM_USER_OUTPUT_SIZE   0x0100

// 4. User Network Settings (256 bytes)
//    Last byte reserved for an 8-bit checksum.
#define EEPROM_USER_NETWORK_START  0x0300
#define EEPROM_USER_NETWORK_SIZE   (sizeof(networkInfo) + 1)

// 5. Sensor Calibration & Thresholds (1024 bytes)
#define EEPROM_SENSOR_CAL_START   0x0400
#define EEPROM_SENSOR_CAL_SIZE    0x0400

// 6. Energy Monitoring Data (2048 bytes)
//    Reserve first 2 bytes for a pointer (wear leveling).
#define EEPROM_ENERGY_MON_START   0x0800
#define EEPROM_ENERGY_MON_SIZE    0x0800
#define ENERGY_MON_POINTER_SIZE   2
#define ENERGY_RECORD_SIZE        16  // Fixed record size

// 7. Event Logs & Fault History (4096 bytes)
//    Reserve first 2 bytes for an append pointer.
#define EEPROM_EVENT_LOG_START    0x1000
#define EEPROM_EVENT_LOG_SIZE     0x1000
#define EVENT_LOG_POINTER_SIZE    2
#define EVENT_LOG_ENTRY_SIZE      32  // Fixed entry size

// 8. User Preferences (4096 bytes)
#define EEPROM_USER_PREF_START    0x2000
#define EEPROM_USER_PREF_SIZE     0x1000

// 9. Reserved for Future Expansion (remaining space)
#define EEPROM_RESERVED_START     0x3000
#define EEPROM_RESERVED_SIZE      (EEPROM_SIZE - EEPROM_RESERVED_START)

#define NETINFO_STATIC  0
#define NETINFO_DHCP    1

typedef struct {
    uint8_t mac[6];   // MAC Address
    uint8_t ip[4];    // IP Address
    uint8_t sn[4];    // Subnet Mask
    uint8_t gw[4];    // Gateway
    uint8_t dns[4];   // DNS Server
    uint8_t dhcp;     // DHCP Mode (0 = Static, 1 = DHCP)
} networkInfo;



// Existing functions for each region
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len);
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len);

int EEPROM_WriteFactoryDefaults(const uint8_t *data, size_t len);
int EEPROM_ReadFactoryDefaults(uint8_t *data, size_t len);

int EEPROM_WriteUserOutput(const uint8_t *data, size_t len);
int EEPROM_ReadUserOutput(uint8_t *data, size_t len);

int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len);
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len);

int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len);
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len);

int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len);
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len);

int EEPROM_WriteEventLogs(const uint8_t *data, size_t len);
int EEPROM_ReadEventLogs(uint8_t *data, size_t len);

int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len);
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len);


// New functions for Data Integrity (Checksum) in critical regions.
// The data length provided must be <= (region size - 1)
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len);
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len);  // returns 0 if valid, -1 if checksum fails

int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info);
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info); 

// New functions for Wear Leveling / Append for Energy Monitoring Data
int EEPROM_AppendEnergyRecord(const uint8_t *data);  // data length fixed as ENERGY_RECORD_SIZE

// New functions for appending Event Logs
int EEPROM_AppendEventLog(const uint8_t *entry);  // entry length fixed as EVENT_LOG_ENTRY_SIZE

networkInfo LoadUserNetworkConfig() ;
#endif // EEPROM_MEMORY_MAP_H
