/**
 * @file EEPROM_MemoryMap.h
 * @author DvidMakesThings - David Sipos
 * @defgroup eeprom EEPROM
 * @brief EEPROM Memory Map (RTOS version)
 * @{
 *
 * @defgroup config05 6. EEPROM Memory Map
 * @ingroup config
 * @brief EEPROM Memory Layout and Data Structures
 * @{
 *
 * @version 2.1.0
 * @date 2025-12-14
 *
 * @details
 * RTOS-compatible EEPROM memory map for CAT24C256 (32KB).
 * This file contains ONLY:
 * - Memory address definitions
 * - Data structures
 * - Default values
 *
 * Memory Map Overview:
 * ```
 * 0x0000 - 0x004F : System Info (firmware version)
 * 0x0030 - 0x0042 : Device Identity (serial number, region) [within sys info]
 * 0x0100 - 0x01FF : Factory Defaults (reserved)
 * 0x0200 - 0x02FF : User Output Presets (names + masks + CRC)
 * 0x0300 - 0x031F : User Network (IP, MAC, etc.)
 * 0x0400 - 0x054F : Sensor Calibration
 * 0x0800 - 0x084F : Temperature Calibration
 * 0x1500 - 0x15FF : Energy Monitoring
 * 0x1600 - 0x17FF : Error Event Log
 * 0x1800 - 0x19FF : Warning Event Log
 * 0x1A00 - 0x1BFF : Channel Labels
 * 0x2000 - 0x21FF : User Preferences
 * 0x7FFE - 0x7FFF : Magic Value
 * ```
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef EEPROM_MEMORY_MAP_H
#define EEPROM_MEMORY_MAP_H

#include "../CONFIG.h"

/* =====================  EEPROM MEMORY MAP  ============================ */

/**
 * @name System Info Block
 * @brief Contains firmware version and other system information.
 * @{
 */
#define EEPROM_SYS_INFO_START 0x0000 /**< Start address of system info block. */
#define EEPROM_SYS_INFO_SIZE 0x0030  /**< Size of system info block in bytes. */
/** @} */

/**
 * @name Device Identity Block
 * @brief Contains serial number and region (within system info block).
 * @details
 * Structure:
 * - Offset 0x00: Serial number (16 bytes, null-terminated)
 * - Offset 0x10: Region code (1 byte)
 * - Offset 0x11: CRC-8 (1 byte)
 * @{
 */
#define EEPROM_DEVICE_IDENTITY_START 0x0030 /**< Start of device identity within sys info. */
#define EEPROM_DEVICE_IDENTITY_SIZE 0x0013  /**< Size: 16 (SN) + 1 (region) + 1 (CRC) + 1 (pad). */
/** @} */

/**
 * @name Factory Defaults (reserved)
 *
 * @note Currently not used
 * @{
 */
#define EEPROM_FACTORY_DEFAULTS_START 0x0100 /**< Start of factory defaults block. */
#define EEPROM_FACTORY_DEFAULTS_SIZE 0x0100  /**< Size of factory defaults block. */
/** @} */

/**
 * @name User Output (relays etc.)
 * @{
 */
#define EEPROM_USER_OUTPUT_START 0x0200 /**< Start of user output presets block. */
#define EEPROM_USER_OUTPUT_SIZE 0x0100  /**< Size of user output presets block. */

/**
 * @name Legacy Relay Power-On States
 * @brief 8-byte legacy array for relay power-on states, stored in the tail of
 *        the user output region to avoid overlapping the presets structure.
 * @{
 */
#define EEPROM_RELAY_STATES_START 0x02F0 /**< Start of 8-byte relay states array. */
#define EEPROM_RELAY_STATES_SIZE 0x0008  /**< Size of relay states array. */
/** @} */
/** @} */

/**
 * @name User Network (with CRC helper)
 * @{
 */
#define EEPROM_USER_NETWORK_START 0x0300 /**< Start of user network block. */
#define EEPROM_USER_NETWORK_SIZE 0x0020  /**< Size of user network block (networkInfo + CRC). */
/** @} */

/**
 * @name Sensor Calibration
 * @brief Generic section for all sensor calibration data.
 * HLW8032 per channel calibration is stored here as hlw_calib_data_t.
 * @{
 */
#define EEPROM_SENSOR_CAL_START 0x0400 /**< Start of sensor calibration block. */
#define EEPROM_SENSOR_CAL_SIZE 0x0150  /**< Size of sensor calibration block. */
/** @} */

/**
 * @name Temperature Sensor Calibration
 * @brief Section for temperature sensor calibration data.
 * @{
 */
#define EEPROM_TEMP_CAL_START 0x0800 /**< Start of sensor calibration block. */
#define EEPROM_TEMP_CAL_SIZE 0x0050  /**< Size of sensor calibration block. */
/** @} */

/**
 * @name Energy Monitoring Data
 *
 * @note Currently not used
 * @{
 */
#define EEPROM_ENERGY_MON_START 0x1500 /**< Start of energy monitoring block. */
#define EEPROM_ENERGY_MON_SIZE 0x0100  /**< Size of energy monitoring block. */
#define ENERGY_MON_POINTER_SIZE 2      /**< Pointer bytes at start of energy block. */
#define ENERGY_RECORD_SIZE 16          /**< Size of one energy record in bytes. */
/** @} */

/**
 * @name Event Logs and Fault History
 * @{
 */
#define EEPROM_EVENT_ERR_START 0x1600  /**< Start of event log block. */
#define EEPROM_EVENT_ERR_SIZE 0x0200   /**< Size of event log block. */
#define EEPROM_EVENT_WARN_START 0x1800 /**< Start of event log block. */
#define EEPROM_EVENT_WARN_SIZE 0x0200  /**< Size of event log block. */
#define EVENT_LOG_POINTER_SIZE 2u      /**< Pointer bytes at start of event log block. */
#define EVENT_LOG_ENTRY_SIZE 2u        /**< Size of one event log entry in bytes. */
/** @} */

/**
 * @name User Preferences
 * @{
 */
#define EEPROM_USER_PREF_START 0x2000 /**< Start of user preferences block. */
#define EEPROM_USER_PREF_SIZE 0x0200  /**< Size of user preferences block. */
/** @} */

/**
 * @name Channel Labels
 * @{
 */
#define EEPROM_CH_LABEL_START 0x1A00 /**< Start of channel labels block. */
#define EEPROM_CH_LABEL_SIZE 0x0200  /**< Size of channel labels block. */
/** @} */

/**
 * @name Reserved Area
 * @{
 */
#define EEPROM_RESERVED_START 0x8000                               /**< Start of reserved area. */
#define EEPROM_RESERVED_SIZE (EEPROM_SIZE - EEPROM_RESERVED_START) /**< Size of reserved area. */
/** @} */

/**
 * @name Magic Value (Factory Init Check)
 * @{
 */
#define EEPROM_MAGIC_ADDR 0x7FFE /**< Magic value address (last word in EEPROM). */
#define EEPROM_MAGIC_VAL 0xA55A  /**< Magic value (factory init complete). */
/** @} */

/* =====================  Data Structures  ============================== */

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

/* =====================  Constants  ==================================== */

/** @brief Constant indicating static network configuration. */
#define EEPROM_NETINFO_STATIC 0
/** @brief Constant indicating DHCP network configuration. */
#define EEPROM_NETINFO_DHCP 1

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

#endif /* EEPROM_MEMORY_MAP_H */

/** @} */