/**
 * @file EEPROM_MemoryMap.h
 * @author Dvid
 * @brief Header for the EEPROM Memory Map and functions.
 * @version 1.6
 * @date 2025-11-02
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef EEPROM_MEMORY_MAP_H
#define EEPROM_MEMORY_MAP_H

#include <stddef.h>
#include <stdint.h>

/**
 * @def EEPROM_SIZE
 * @brief Total EEPROM size in bytes.
 */
#define EEPROM_SIZE 65536

/* =====================  EEPROM MEMORY MAP  ============================ */

/**
 * @name System Info (SN, SW)
 * @{
 */
#define EEPROM_SYS_INFO_START 0x0000 /**< Start address of system info block. */
#define EEPROM_SYS_INFO_SIZE 0x0100  /**< Size of system info block in bytes. */
/** @} */

/**
 * @name Factory Defaults (reserved)
 * @{
 */
#define EEPROM_FACTORY_DEFAULTS_START 0x0100 /**< Start of factory defaults block. */
#define EEPROM_FACTORY_DEFAULTS_SIZE 0x0100  /**< Size of factory defaults block. */
/** @} */

/**
 * @name User Output (relays etc.)
 * @{
 */
#define EEPROM_USER_OUTPUT_START 0x0200 /**< Start of user output block. */
#define EEPROM_USER_OUTPUT_SIZE 0x0100  /**< Size of user output block. */
/** @} */

/**
 * @name User Network (with CRC helper)
 * @{
 */
#define EEPROM_USER_NETWORK_START 0x0300                   /**< Start of user network block. */
#define EEPROM_USER_NETWORK_SIZE (sizeof(networkInfo) + 1) /**< Size of user network block. */
/** @} */

/**
 * @name Sensor Calibration
 * @brief Generic section for all sensor calibration data.
 * HLW8032 per channel calibration is stored here as hlw_calib_data_t.
 * @{
 */
#define EEPROM_SENSOR_CAL_START 0x0400 /**< Start of sensor calibration block. */
#define EEPROM_SENSOR_CAL_SIZE 0x0400  /**< Size of sensor calibration block. */
/** @} */

/**
 * @name Energy Monitoring Data
 * @{
 */
#define EEPROM_ENERGY_MON_START 0x0800 /**< Start of energy monitoring block. */
#define EEPROM_ENERGY_MON_SIZE 0x0800  /**< Size of energy monitoring block. */
#define ENERGY_MON_POINTER_SIZE 2      /**< Pointer bytes at start of energy block. */
#define ENERGY_RECORD_SIZE 16          /**< Size of one energy record in bytes. */
/** @} */

/**
 * @name Event Logs and Fault History
 * @{
 */
#define EEPROM_EVENT_LOG_START 0x1000 /**< Start of event log block. */
#define EEPROM_EVENT_LOG_SIZE 0x1000  /**< Size of event log block. */
#define EVENT_LOG_POINTER_SIZE 2      /**< Pointer bytes at start of event log block. */
#define EVENT_LOG_ENTRY_SIZE 32       /**< Size of one event log entry in bytes. */
/** @} */

/**
 * @name User Preferences
 * @{
 */
#define EEPROM_USER_PREF_START 0x2000 /**< Start of user preferences block. */
#define EEPROM_USER_PREF_SIZE 0x1000  /**< Size of user preferences block. */
/** @} */

/**
 * @name Channel Labels
 * @{
 */
#define EEPROM_CH_LABEL_START 0x3000 /**< Start of channel labels block. */
#define EEPROM_CH_LABEL_SIZE 0x3000  /**< Size of channel labels block. */
/** @} */

/**
 * @name Reserved Area
 * @{
 */
#define EEPROM_RESERVED_START 0x6000                               /**< Start of reserved area. */
#define EEPROM_RESERVED_SIZE (EEPROM_SIZE - EEPROM_RESERVED_START) /**< Size of reserved area. */
/** @} */

/* =====================  Structs  ===================================== */

/**
 * @struct networkInfo
 * @brief Stores network settings for the device.
 */
typedef struct {
    uint8_t mac[6]; /**< MAC address bytes. */
    uint8_t ip[4];  /**< IPv4 address. */
    uint8_t sn[4];  /**< Subnet mask. */
    uint8_t gw[4];  /**< Default gateway. */
    uint8_t dns[4]; /**< DNS server. */
    uint8_t dhcp;   /**< DHCP mode, 0 = static, 1 = DHCP. */
} networkInfo;

/**
 * @struct userPrefInfo
 * @brief User preferences such as device name and temperature unit.
 */
typedef struct {
    char device_name[32]; /**< UTF-8 device name. */
    char location[32];    /**< UTF-8 location string. */
    uint8_t temp_unit;    /**< 0=C, 1=F, 2=K. */
} userPrefInfo;

/**
 * @struct hlw_calib_t
 * @brief HLW8032 calibration data per channel.
 */
typedef struct {
    float voltage_factor;    /**< VF calibration factor. */
    float current_factor;    /**< CF calibration factor. */
    float voltage_offset;    /**< Zero-point voltage offset in volts. */
    float current_offset;    /**< Zero-point current offset in amps. */
    float r1_actual;         /**< Actual R1 in ohms. */
    float r2_actual;         /**< Actual R2 in ohms. */
    float shunt_actual;      /**< Actual shunt in ohms. */
    uint8_t calibrated;      /**< 0xCA if calibrated, 0xFF otherwise. */
    uint8_t zero_calibrated; /**< 0xCA if zero-cal done, 0xFF otherwise. */
    uint8_t reserved[5];     /**< Reserved bytes for alignment. */
} hlw_calib_t;

/**
 * @struct hlw_calib_data_t
 * @brief Container for all channel calibration data.
 */
typedef struct {
    hlw_calib_t channels[8]; /**< Per channel calibration records. */
    uint8_t crc;             /**< Reserved CRC byte, not used yet. */
} hlw_calib_data_t;

/* =====================  Defaults  ==================================== */

/**
 * @brief Default relay status array, 1 byte per channel.
 */
static const uint8_t DEFAULT_RELAY_STATUS[8] = {0};

/** @brief Constant indicating static network configuration. */
#define EEPROM_NETINFO_STATIC 0
/** @brief Constant indicating DHCP network configuration. */
#define EEPROM_NETINFO_DHCP 1

/**
 * @brief Default network configuration.
 */
static const networkInfo DEFAULT_NETWORK = {.ip = {192, 168, 0, 11},
                                            .gw = {192, 168, 0, 1},
                                            .sn = {255, 255, 255, 0},
                                            .mac = {0x00, 0x08, 0xDC, 0xBE, 0xEF, 0x91},
                                            .dns = {8, 8, 8, 8},
                                            .dhcp = EEPROM_NETINFO_STATIC};

/** @brief Default placeholder energy data. */
static const uint8_t DEFAULT_ENERGY_DATA[64] = {0};
/** @brief Default placeholder event log data. */
static const uint8_t DEFAULT_LOG_DATA[64] = {0};
/** @brief Default placeholder user pref data. */
static const uint8_t DEFAULT_USER_PREF[64] = {0};

/* =====================  Prototypes  ================================== */

/**
 * @brief Erase the entire EEPROM to 0xFF.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_EraseAll(void);

/* ---- System Info ---- */

/**
 * @brief Write raw system info bytes.
 * @param data Pointer to buffer.
 * @param len Number of bytes, must be <= EEPROM_SYS_INFO_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len);

/**
 * @brief Read raw system info bytes.
 * @param data Output buffer.
 * @param len Number of bytes, must be <= EEPROM_SYS_INFO_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len);

/**
 * @brief Write system info with CRC-8 appended.
 * @param data Payload buffer, length must be <= SIZE-1.
 * @param len Payload length.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len);

/**
 * @brief Read system info and verify CRC-8.
 * @param data Output payload buffer.
 * @param len Payload length to read and verify.
 * @return 0 if CRC matches, -1 on mismatch or error.
 */
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len);

/* ---- Factory defaults check/write ---- */

/**
 * @brief Write factory defaults to all sections.
 * @return 0 on success, -1 if any write fails.
 */
int EEPROM_WriteFactoryDefaults(void);

/**
 * @brief Validate presence and basic integrity of key sections.
 * @return 0 always, logs errors via diagnostics.
 */
int EEPROM_ReadFactoryDefaults(void);

/* ---- User Output ---- */

/**
 * @brief Write user output configuration bytes.
 * @param data Pointer to buffer.
 * @param len Number of bytes, <= EEPROM_USER_OUTPUT_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len);

/**
 * @brief Read user output configuration bytes.
 * @param data Output buffer.
 * @param len Number of bytes, <= EEPROM_USER_OUTPUT_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ReadUserOutput(uint8_t *data, size_t len);

/* ---- Network (raw + CRC helpers) ---- */

/**
 * @brief Write raw user network configuration.
 * @param data Pointer to buffer.
 * @param len Number of bytes, <= EEPROM_USER_NETWORK_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len);

/**
 * @brief Read raw user network configuration.
 * @param data Output buffer.
 * @param len Number of bytes, <= EEPROM_USER_NETWORK_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len);

/**
 * @brief Write user network configuration with CRC-8.
 * @param net_info Pointer to networkInfo struct.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info);

/**
 * @brief Read user network configuration and verify CRC-8.
 * @param net_info Output networkInfo struct.
 * @return 0 if CRC ok, -1 on failure.
 */
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info);

/* ---- Sensor Calibration (generic, no CRC yet) ---- */

/**
 * @brief Write sensor calibration blob.
 * @param data Pointer to blob (e.g., hlw_calib_data_t).
 * @param len Blob size, <= EEPROM_SENSOR_CAL_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len);

/**
 * @brief Write a single channel's calibration record to EEPROM with 5-digit precision.
 *        The entire record is written atomically to its slot.
 * @param ch  Channel index 0..7
 * @param in  Input calibration record
 * @return 0 on success, -1 on param error
 */
int EEPROM_WriteSensorCalibrationForChannel(uint8_t ch, const hlw_calib_t *in);

/**
 * @brief Read sensor calibration blob.
 * @param data Output buffer.
 * @param len Number of bytes to read, <= EEPROM_SENSOR_CAL_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len);

/**
 * @brief Read a single channel's calibration record from EEPROM and apply sane defaults.
 *        Uses the per-channel slot layout inside the Sensor Calibration region.
 * @param ch  Channel index 0..7
 * @param out Output calibration record
 * @return 0 on success, -1 on param error
 */
int EEPROM_ReadSensorCalibrationForChannel(uint8_t ch, hlw_calib_t *out);

/* ---- Energy Monitoring ---- */

/**
 * @brief Write energy monitoring data.
 * @param data Pointer to buffer.
 * @param len Number of bytes, <= EEPROM_ENERGY_MON_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len);

/**
 * @brief Read energy monitoring data.
 * @param data Output buffer.
 * @param len Number of bytes, <= EEPROM_ENERGY_MON_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len);

/**
 * @brief Append one fixed-size energy record with internal pointer management.
 * @param data Pointer to record buffer of size ENERGY_RECORD_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_AppendEnergyRecord(const uint8_t *data);

/* ---- Event Logs ---- */

/**
 * @brief Write event logs area.
 * @param data Pointer to buffer.
 * @param len Number of bytes, <= EEPROM_EVENT_LOG_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len);

/**
 * @brief Read event logs area.
 * @param data Output buffer.
 * @param len Number of bytes, <= EEPROM_EVENT_LOG_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len);

/**
 * @brief Append one fixed-size event log entry with internal pointer management.
 * @param entry Pointer to entry buffer of size EVENT_LOG_ENTRY_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_AppendEventLog(const uint8_t *entry);

/* ---- User Prefs (raw + CRC helpers) ---- */

/**
 * @brief Write raw user preferences.
 * @param data Pointer to buffer.
 * @param len Number of bytes, <= EEPROM_USER_PREF_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len);

/**
 * @brief Read raw user preferences.
 * @param data Output buffer.
 * @param len Number of bytes, <= EEPROM_USER_PREF_SIZE.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len);

/**
 * @brief Write user preferences with CRC-8.
 * @param prefs Pointer to userPrefInfo struct.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteUserPrefsWithChecksum(const userPrefInfo *prefs);

/**
 * @brief Read user preferences and verify CRC-8.
 * @param prefs Output userPrefInfo struct.
 * @return 0 if CRC ok, -1 on failure.
 */
int EEPROM_ReadUserPrefsWithChecksum(userPrefInfo *prefs);

/**
 * @brief Load user preferences, fallback to defaults on CRC failure.
 * @return Loaded userPrefInfo struct.
 */
userPrefInfo LoadUserPreferences(void);

/**
 * @brief Write default device name and location with CRC.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteDefaultNameLocation(void);

/* ---- Convenience loader ---- */

/**
 * @brief Load user network configuration, fallback to defaults if CRC fails.
 * @return Loaded networkInfo struct.
 */
networkInfo LoadUserNetworkConfig(void);

/* ---- Channel labels (per channel) ---- */

/**
 * @def ENERGIS_NUM_CHANNELS
 * @brief Number of PDU output channels.
 */
#ifndef ENERGIS_NUM_CHANNELS
#define ENERGIS_NUM_CHANNELS 8u
#endif

/**
 * @def EEPROM_CH_LABEL_SLOT
 * @brief Slot size in bytes per channel label.
 */
#define EEPROM_CH_LABEL_SLOT (EEPROM_CH_LABEL_SIZE / ENERGIS_NUM_CHANNELS)

/**
 * @brief Write one channel label string into its slot.
 * @param channel_index Channel index 0..ENERGIS_NUM_CHANNELS-1.
 * @param label Zero-terminated UTF-8 label.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_WriteChannelLabel(uint8_t channel_index, const char *label);

/**
 * @brief Read one channel label string from its slot.
 * @param channel_index Channel index 0..ENERGIS_NUM_CHANNELS-1.
 * @param out Output buffer for label.
 * @param out_len Size of output buffer in bytes.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ReadChannelLabel(uint8_t channel_index, char *out, size_t out_len);

/**
 * @brief Clear one channel label slot to zeros.
 * @param channel_index Channel index 0..ENERGIS_NUM_CHANNELS-1.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ClearChannelLabel(uint8_t channel_index);

/**
 * @brief Clear all channel label slots.
 * @return 0 on success, -1 on failure.
 */
int EEPROM_ClearAllChannelLabels(void);

#endif /* EEPROM_MEMORY_MAP_H */
