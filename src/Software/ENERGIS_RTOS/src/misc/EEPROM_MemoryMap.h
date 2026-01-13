/**
 * @file src/misc/EEPROM_MemoryMap.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup config06 6. EEPROM Memory Map
 * @ingroup config
 * @brief EEPROM Memory Layout and Data Structures
 * @{
 *
 * @version 2.1.0
 * @date 2025-12-14
 *
 * @details
 * This file serves as the single authoritative source for all EEPROM memory layout
 * definitions, address assignments, and data structures. It provides a centralized
 * memory map for the CAT24C256 32KB I2C EEPROM used in the ENERGIS PDU system.
 *
 * Key Features:
 * - Single source of truth for all EEPROM addresses
 * - Hierarchical memory block organization
 * - Data structure definitions for all EEPROM regions
 * - Constants for network, calibration, and logging parameters
 * - Version tracking for memory map evolution
 *
 * Design Principles:
 * - NO MODULE may define EEPROM addresses locally
 * - ALL address definitions MUST be in this file
 * - Memory regions allocated with gaps for future expansion
 * - Page alignment considerations for CAT24C256 (64-byte pages)
 * - Ring buffer regions include pointer storage
 * - CRC validation integrated into critical sections
 *
 * EEPROM Hardware Specifications:
 * - Device: CAT24C256 (Microchip/On Semi)
 * - Capacity: 32KB (32,768 bytes)
 * - Address range: 0x0000 - 0x7FFF
 * - Page size: 64 bytes
 * - Write cycle time: 5ms typical
 * - Endurance: 1,000,000 write cycles
 * - Data retention: 200 years
 * - I2C address: Configurable via A0-A2 pins
 *
 * Memory Map Overview:
 * ```
 * 0x0000 - 0x002F  System Info (48 bytes)
 *                   Firmware version string
 * 0x0030 - 0x0042  Device Identity (19 bytes, within sys info)
 *                   Serial number (16) + Region (1) + CRC (1) + 1
 * 0x0043 - 0x00FF  [Gap for System Info expansion]
 * 0x0100 - 0x01FF  Factory Defaults (256 bytes, reserved)
 * 0x0200 - 0x02EF  User Output Presets (240 bytes)
 *                   5 presets × 28 bytes + header + CRC16
 * 0x02F0 - 0x02F7  Legacy Relay States (8 bytes)
 * 0x02F8 - 0x02FF  [Gap for User Output expansion]
 * 0x0300 - 0x031F  User Network (32 bytes)
 *                   MAC(6) + IP(4) + Subnet(4) + GW(4) + DNS(4) + DHCP(1) + CRC8(1)
 * 0x0320 - 0x03FF  [Gap for Network expansion]
 * 0x0400 - 0x054F  Sensor Calibration (336 bytes)
 *                   8 channels × hlw_calib_t (41 bytes) + CRC
 * 0x0550 - 0x07FF  [Gap for Sensor expansion]
 * 0x0800 - 0x084F  Temperature Calibration (80 bytes)
 *                   temp_calib_t with magic + version + CRC32
 * 0x0850 - 0x14FF  [Gap for Calibration expansion]
 * 0x1500 - 0x15FF  Energy Monitoring (256 bytes)
 *                   Ring buffer: Pointer(2) + Records(254)
 * 0x1600 - 0x17FF  Error Event Log (512 bytes)
 *                   Ring buffer: Pointer(2) + Entries(510)
 * 0x1800 - 0x19FF  Warning Event Log (512 bytes)
 *                   Ring buffer: Pointer(2) + Entries(510)
 * 0x1A00 - 0x1BFF  Channel Labels (512 bytes)
 *                   8 channels × 64 bytes per label slot
 * 0x1C00 - 0x1FFF  [Gap for Logging/Labels expansion]
 * 0x2000 - 0x21FF  User Preferences (512 bytes)
 *                   Device name(32) + Location(32) + Unit(1) + CRC
 * 0x2200 - 0x7FFD  [Reserved for Future Use]
 * 0x7FFE - 0x7FFF  Magic Value (2 bytes)
 *                   0xA55A indicates factory init complete
 *
 * Total Used: ~9KB / 32KB (72% available for expansion)
 * ```
 *
 * Memory Region Details:
 *
 * 1. System Info (0x0000-0x002F, 48 bytes):
 *    - Purpose: Firmware version identification
 *    - Contents: SWVERSION string from config.h
 *    - Format: Null-terminated ASCII string
 *    - Updated: During firmware updates
 *
 * 2. Device Identity (0x0030-0x0042, 19 bytes):
 *    - Purpose: Unique device identification and region setting
 *    - Contents: Serial number (16) + Region (1) + CRC-8 (1) + Pad (1)
 *    - Protection: CRC-8 validation
 *    - Provisioning: Written via UART commands after manufacture
 *
 * 3. Factory Defaults (0x0100-0x01FF, 256 bytes):
 *    - Purpose: Reserved for future factory configuration storage
 *    - Status: Currently unused
 *    - Intended use: Manufacturing test results, hardware revision
 *
 * 4. User Output Presets (0x0200-0x02FF, 256 bytes):
 *    - Purpose: User-defined relay configuration presets
 *    - Contents: Header + 5 presets (28 bytes each) + CRC16
 *    - Features: Apply-on-startup configuration
 *    - Format: Header magic + startup ID + preset array + CRC16
 *
 * 5. Legacy Relay States (0x02F0-0x02F7, 8 bytes):
 *    - Purpose: Backward compatibility with older firmware
 *    - Contents: 8-byte array for relay power-on states
 *    - Location: Tail of user output region to avoid overlap
 *
 * 6. User Network (0x0300-0x031F, 32 bytes):
 *    - Purpose: Network configuration parameters
 *    - Contents: MAC address + IPv4 settings + DHCP flag + CRC-8
 *    - Protection: CRC-8 validation, MAC repair on corruption
 *    - Updates: Web interface, SNMP, console commands
 *
 * 7. Sensor Calibration (0x0400-0x054F, 336 bytes):
 *    - Purpose: HLW8032 power meter calibration per channel
 *    - Contents: 8 × hlw_calib_t structures + CRC
 *    - Updates: UART calibration workflow
 *    - Factors: Voltage factor, current factor, offsets, resistor values
 *
 * 8. Temperature Calibration (0x0800-0x084F, 80 bytes):
 *    - Purpose: RP2040 internal temperature sensor calibration
 *    - Contents: temp_calib_t with magic + version + CRC32
 *    - Modes: 1-point (offset) or 2-point (slope + intercept)
 *    - Updates: UART calibration commands
 *
 * 9. Energy Monitoring (0x1500-0x15FF, 256 bytes):
 *    - Purpose: Historical energy consumption logging
 *    - Structure: Ring buffer with write pointer
 *    - Layout: Pointer (2 bytes) + Records (16 bytes each)
 *    - Capacity: 15 energy records
 *
 * 10. Error Event Log (0x1600-0x17FF, 512 bytes):
 *     - Purpose: Critical error event history
 *     - Structure: Ring buffer with write pointer
 *     - Layout: Pointer (2 bytes) + Entries (2 bytes each)
 *     - Capacity: 255 error codes
 *
 * 11. Warning Event Log (0x1800-0x19FF, 512 bytes):
 *     - Purpose: Non-critical warning event history
 *     - Structure: Ring buffer with write pointer
 *     - Layout: Pointer (2 bytes) + Entries (2 bytes each)
 *     - Capacity: 255 warning codes
 *
 * 12. Channel Labels (0x1A00-0x1BFF, 512 bytes):
 *     - Purpose: User-defined text labels for output channels
 *     - Structure: 8 fixed-size slots (64 bytes each)
 *     - Format: Null-terminated strings (max 25 characters)
 *     - Caching: Loaded to RAM on startup
 *
 * 13. User Preferences (0x2000-0x21FF, 512 bytes):
 *     - Purpose: Device name, location, temperature unit
 *     - Contents: Device name (32) + Location (32) + Unit (1) + CRC-8 (1)
 *     - Protection: CRC-8 validation, split writes for page safety
 *     - Display: Web interface, SNMP sysName/sysLocation
 *
 * 14. Magic Value (0x7FFE-0x7FFF, 2 bytes):
 *     - Purpose: Factory initialization completion marker
 *     - Value: 0xA55A indicates all defaults written
 *     - Check: Performed on first boot to trigger factory default write
 *
 * Memory Allocation Strategy:
 * - Frequently updated regions (event logs, energy) isolated from static config
 * - Large gaps between regions allow for growth without relocation
 * - Magic value at end prevents accidental detection from blank EEPROM
 * - Page-aligned writes where possible to maximize CAT24C256 efficiency
 * - Critical config protected with CRC-8 or CRC16
 *
 * Page Alignment Considerations:
 * - CAT24C256 has 64-byte pages (addresses 0x00-0x3F, 0x40-0x7F, etc.)
 * - Writes spanning page boundary require multiple write cycles
 * - Large structures written in page-aligned chunks when possible
 * - Split-write strategy used for user preferences to respect boundaries
 *
 * Version History:
 * - v1.x: Original memory map with compile-time serial number
 * - v2.0: Serial number moved to UART provisioning
 * - v2.1: User output presets added, relay states moved to legacy region
 *
 * Thread Safety:
 * - All EEPROM access requires eepromMtx held by caller
 * - Managed by StorageTask and storage submodules
 * - Prevents concurrent read/modify/write races
 *
 * Integration Points:
 * - storage_submodule/*: Use address definitions for all I/O
 * - factory_defaults: Initializes all regions on first boot
 * - StorageTask: Coordinates access via eepromMtx
 * - CAT24C256 driver: Low-level I2C read/write operations
 *
 * Usage Rules:
 * - Include this file in all modules requiring EEPROM access
 * - NEVER hardcode EEPROM addresses in other files
 * - Use defined constants exclusively
 * - Proposal for new regions requires memory map update here first
 * - Document all address changes in version history
 *
 * @note This file contains ONLY definitions and structures, NO function implementations.
 * @note All address definitions are in hexadecimal for readability.
 * @note Memory regions include expansion gaps to avoid future relocation.
 * @note Ring buffers include 2-byte pointer at start of each region.
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
 * @brief Firmware version identification and system metadata storage.
 *
 * @details
 * Stores the firmware version string at the beginning of EEPROM for easy identification.
 * The version string is read from SWVERSION compile-time constant and written during
 * factory defaults initialization. Used for firmware update validation and diagnostics.
 *
 * Contents:
 * - Firmware version string (SWVERSION from config.h)
 * - Format: Null-terminated ASCII string
 * - Example: "ENERGIS_v2.1.0_2025-12-14"
 * - Updated: During firmware updates or factory reset
 *
 * Access Pattern:
 * - Written: Factory defaults, firmware update
 * - Read: Boot diagnostics, web interface, SNMP sysDescr
 * - Frequency: Write once per firmware update, read on boot
 *
 * @{
 */
#define EEPROM_SYS_INFO_START 0x0000 /**< Start address of system info block. */
#define EEPROM_SYS_INFO_SIZE 0x0030  /**< Size of system info block (48 bytes). */
/** @} */

/**
 * @name Device Identity Block
 * @brief Unique device identification and region configuration storage.
 *
 * @details
 * Stores device-specific parameters that uniquely identify each PDU unit and determine
 * its operational region (EU/US). This block resides within the system info region but
 * is managed separately via UART provisioning commands.
 *
 * Structure Layout (19 bytes total):
 * - Offset 0x00: Serial number (16 bytes, null-terminated string)
 * - Offset 0x10: Region code (1 byte, DEVICE_REGION_EU or DEVICE_REGION_US)
 * - Offset 0x11: CRC-8 checksum (1 byte, validates serial + region)
 * - Offset 0x12: Reserved padding (1 byte, for alignment)
 *
 * Serial Number Format:
 * - Up to 15 ASCII characters + null terminator
 * - Typically: "ENR" + year + production sequence
 * - Example: "ENR2025A001234"
 * - Must be unique per device for MAC address derivation
 *
 * Region Codes:
 * - DEVICE_REGION_EU (0x45, 'E'): 10A current limit (IEC/ENEC)
 * - DEVICE_REGION_US (0x55, 'U'): 15A current limit (UL/CSA)
 * - DEVICE_REGION_UNKNOWN (0x00): Unprovisioned device
 *
 * Provisioning Workflow:
 * 1. Device ships with placeholder "UNPROVISIONED" serial
 * 2. Factory uses UART commands to unlock provisioning window
 * 3. Serial number and region written via PROV commands
 * 4. CRC-8 calculated and stored for validation
 * 5. MAC address automatically derived from new serial
 *
 * CRC Validation:
 * - Polynomial: 0x07 (via calculate_crc8)
 * - Input: Serial number (16 bytes) + Region (1 byte)
 * - Stored at offset 0x11
 * - Validated on boot and before MAC generation
 *
 * Integration:
 * - MAC address generation: Uses FNV-1a hash of serial number
 * - Overcurrent protection: Region determines current limit
 * - Web interface: Displays serial number for identification
 * - SNMP: Serial exposed via custom OID
 *
 * @{
 */
#define EEPROM_DEVICE_IDENTITY_START 0x0030 /**< Start of device identity within sys info. */
#define EEPROM_DEVICE_IDENTITY_SIZE                                                                \
    0x0013 /**< Size: 16 (SN) + 1 (region) + 1 (CRC) + 1 (pad) = 19 bytes. */
/** @} */

/**
 * @name Factory Defaults Block (Reserved)
 * @brief Reserved region for future factory configuration data.
 *
 * @details
 * This 256-byte region is allocated but currently unused. Reserved for future expansion
 * of factory-specific data that should not be user-modifiable.
 *
 * Intended Future Use:
 * - Manufacturing test results and QA data
 * - Hardware revision and BOM variant identifiers
 * - Factory calibration certificates
 * - Production date and facility codes
 * - Tamper-evident checksums
 *
 * @note Currently not used by any module.
 * @note Write access reserved for factory provisioning tools.
 * @note User firmware should treat this region as read-only.
 *
 * @{
 */
#define EEPROM_FACTORY_DEFAULTS_START 0x0100 /**< Start of factory defaults block. */
#define EEPROM_FACTORY_DEFAULTS_SIZE 0x0100  /**< Size of factory defaults block (256 bytes). */
/** @} */

/**
 * @name User Output Presets Block
 * @brief User-configurable relay configuration presets with apply-on-startup feature.
 *
 * @details
 * Allows users to save up to 5 named relay configurations for quick recall. One preset
 * can be designated to apply automatically on device startup. Includes header with magic
 * value and CRC16 for data integrity.
 *
 * Structure Layout (256 bytes total):
 * - Header (4 bytes):
 *   * Magic byte (0xC5): Data validity marker
 *   * Startup preset ID (0-4 or 0xFF for none)
 *   * Reserved (2 bytes): Future expansion
 * - Presets array (140 bytes = 5 × 28 bytes):
 *   * Each preset: Name (26 bytes) + Relay mask (1 byte) + Valid flag (1 byte)
 * - CRC16 (2 bytes): Checksum over header + presets (144 bytes)
 *
 * Preset Structure (28 bytes each):
 * - Name: 25-character null-terminated string
 * - Null terminator: Byte 26
 * - Relay mask: 8-bit mask (bit N = channel N state, 1=ON 0=OFF)
 * - Valid flag: 0xA5 if preset used, else empty slot
 *
 * Apply-on-Startup Feature:
 * - Startup preset ID specifies which preset to apply after boot
 * - Value 0-4: Apply corresponding preset
 * - Value 0xFF: No automatic application (use last saved states)
 * - Applied before normal operation begins
 *
 * Access Pattern:
 * - Written: Web interface preset save/delete, console commands
 * - Read: Boot time (apply-on-startup), web interface preset list
 * - Frequency: Occasional user configuration changes
 *
 * @{
 */
#define EEPROM_USER_OUTPUT_START 0x0200 /**< Start of user output presets block. */
#define EEPROM_USER_OUTPUT_SIZE 0x0100  /**< Size of user output presets block (256 bytes). */

/**
 * @name Legacy Relay Power-On States
 * @brief Backward compatibility storage for older firmware versions.
 *
 * @details
 * 8-byte array storing per-channel power-on states for compatibility with firmware
 * versions prior to preset implementation. Located at tail of user output region
 * to avoid conflict with preset structure.
 *
 * Structure Layout (8 bytes):
 * - Byte 0-7: Power-on state for channels 0-7 (0=OFF, 1=ON)
 *
 * @note Deprecated: New firmware uses preset system.
 * @note Location chosen to avoid overlap with preset structure.
 * @note Read during firmware migration for preset conversion.
 *
 * @{
 */
#define EEPROM_RELAY_STATES_START 0x02F0 /**< Start of 8-byte relay states array. */
#define EEPROM_RELAY_STATES_SIZE 0x0008  /**< Size of relay states array (8 bytes). */
/** @} */
/** @} */

/**
 * @name User Network Configuration Block
 * @brief Network parameters with CRC-8 validation and automatic MAC repair.
 *
 * @details
 * Stores complete network configuration including MAC address, IPv4 settings, and DHCP
 * mode. Protected by CRC-8 checksum with automatic MAC address repair on corruption.
 *
 * Structure Layout (32 bytes total):
 * - Offset 0-5:   MAC address (6 bytes)
 * - Offset 6-9:   IPv4 address (4 bytes)
 * - Offset 10-13: Subnet mask (4 bytes)
 * - Offset 14-17: Default gateway (4 bytes)
 * - Offset 18-21: DNS server (4 bytes)
 * - Offset 22:    DHCP enable (1 byte, 0=static 1=DHCP)
 * - Offset 23:    CRC-8 checksum (1 byte)
 * - Offset 24-31: Unused (reserved for expansion)
 *
 * MAC Address Management:
 * - Format: ENERGIS_MAC_PREFIX (3 bytes) + Serial hash (3 bytes)
 * - Derived from device serial number via FNV-1a hash
 * - Automatically repaired if corruption detected on boot
 * - Repair checks: Invalid prefix, all-zero/0xFF suffix, serial mismatch
 *
 * Default Configuration:
 * - IP: 192.168.1.100
 * - Subnet: 255.255.255.0
 * - Gateway: 192.168.1.1
 * - DNS: 8.8.8.8
 * - DHCP: Disabled (static IP)
 * - MAC: Derived from serial number
 *
 * CRC Validation:
 * - Polynomial: 0x07 (via calculate_crc8)
 * - Input: Bytes 0-22 (MAC through DHCP flag)
 * - Stored at offset 23
 * - Failed CRC triggers fallback to defaults
 *
 * Access Pattern:
 * - Written: Web interface, SNMP, console commands, first boot
 * - Read: Boot time (NetTask initialization), web interface
 * - Frequency: Occasional user configuration changes
 *
 * @{
 */
#define EEPROM_USER_NETWORK_START 0x0300 /**< Start of user network block. */
#define EEPROM_USER_NETWORK_SIZE 0x0020  /**< Size of user network block (32 bytes). */
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
 * @brief Network configuration data structure for EEPROM storage.
 *
 * @details
 * Packs all network parameters into a compact structure for EEPROM persistence.
 * Used with CRC-8 checksum for data integrity validation. MAC address automatically
 * derived from device serial number and repaired if corrupted.
 *
 * Fields:
 * - mac: 6-byte hardware address (ENERGIS_MAC_PREFIX + FNV-1a hash of serial)
 * - ip: 4-byte IPv4 address in network byte order
 * - sn: 4-byte subnet mask in network byte order
 * - gw: 4-byte default gateway address in network byte order
 * - dns: 4-byte DNS server address in network byte order
 * - dhcp: 1-byte flag (0=static IP, 1=DHCP client mode)
 *
 * Usage:
 * - Populated from EEPROM on boot via LoadUserNetworkConfig()
 * - Passed to W5500 Ethernet driver for chip configuration
 * - Modified via web interface, SNMP, or console commands
 * - Persisted to EEPROM with CRC-8 via EEPROM_WriteUserNetworkWithChecksum()
 *
 * @note Structure size: 23 bytes (before CRC appended).
 * @note CRC-8 calculated over entire structure and stored separately.
 * @note MAC address repaired automatically if corrupted.
 */
typedef struct {
    /** @ingroup config06 */
    uint8_t mac[6]; /**< MAC address (6 bytes). */
    uint8_t ip[4];  /**< IPv4 address (4 bytes). */
    uint8_t sn[4];  /**< Subnet mask (4 bytes). */
    uint8_t gw[4];  /**< Default gateway (4 bytes). */
    uint8_t dns[4]; /**< DNS server (4 bytes). */
    uint8_t dhcp;   /**< DHCP mode: 0=static, 1=DHCP (1 byte). */
} networkInfo;

/**
 * @struct userPrefInfo
 * @brief User preferences data structure for device identification and display settings.
 *
 * @details
 * Stores user-configurable device metadata and display preferences. Used for SNMP
 * sysName/sysLocation, web interface identification, and temperature unit conversion.
 * Protected by CRC-8 checksum with automatic fallback to defaults on corruption.
 *
 * Fields:
 * - device_name: 32-byte null-terminated device identification string
 *   * Default: "ENERGIS PDU"
 *   * Used in: SNMP sysName, web interface header
 *   * Max: 31 displayable characters + null terminator
 *
 * - location: 32-byte null-terminated physical location string
 *   * Default: "Unknown"
 *   * Used in: SNMP sysLocation, web interface
 *   * Max: 31 displayable characters + null terminator
 *
 * - temp_unit: 1-byte temperature display unit selector
 *   * 0 = Celsius (default)
 *   * 1 = Fahrenheit
 *   * 2 = Kelvin (reserved for future use)
 *   * Applied to: Web interface, SNMP temperature OIDs
 *
 * Storage Format:
 * - Total size: 65 bytes (before CRC appended)
 * - EEPROM layout: device_name(32) + location(32) + temp_unit(1) + CRC-8(1)
 * - CRC calculated over first 65 bytes
 * - Split writes used to respect CAT24C256 page boundaries
 *
 * Usage:
 * - Loaded from EEPROM via LoadUserPreferences() on boot
 * - Modified via web interface or console commands
 * - Persisted with CRC-8 via EEPROM_WriteUserPrefsWithChecksum()
 * - Defaults applied on CRC mismatch or first boot
 *
 * @note Structure size: 65 bytes (before CRC appended).
 * @note Strings always null-terminated for safety.
 * @note UTF-8 encoding supported for international characters.
 */
typedef struct {
    /** @ingroup config06 */
    char device_name[32]; /**< Device name (32 bytes, null-terminated). */
    char location[32];    /**< Physical location (32 bytes, null-terminated). */
    uint8_t temp_unit;    /**< Temperature unit: 0=Celsius, 1=Fahrenheit, 2=Kelvin (1 byte). */
} userPrefInfo;

/**
 * @struct hlw_calib_t
 * @brief Per-channel calibration parameters for HLW8032 power meter IC.
 *
 * @details
 * Stores calibration factors and hardware parameters for one HLW8032 power measurement
 * channel. Each PDU has 8 independent channels requiring individual calibration.
 * Calibration performed via UART commands using known voltage and current references.
 *
 * Calibration Factors:
 * - voltage_factor: VF multiplier for voltage calculation (default: 0.596)
 *   * Converts HLW8032 voltage register to actual volts
 *   * Derived from: V_actual / V_register during calibration
 *
 * - current_factor: CF multiplier for current calculation (default: 14.871)
 *   * Converts HLW8032 current register to actual amps
 *   * Derived from: I_actual / I_register during calibration
 *
 * Zero-Point Offsets:
 * - voltage_offset: Zero-load voltage correction in volts
 *   * Compensates for ADC offset error
 *   * Measured with no load, subtracted from readings
 *
 * - current_offset: Zero-load current correction in amps
 *   * Compensates for ADC offset and leakage
 *   * Measured with no load, subtracted from readings
 *
 * Hardware Parameters:
 * - r1_actual: Measured high-side voltage divider resistor (default: 1880kΩ)
 * - r2_actual: Measured low-side voltage divider resistor (default: 1kΩ)
 * - shunt_actual: Measured current sense shunt resistor (default: 0.001Ω)
 *
 * Calibration Flags:
 * - calibrated: 0xCA if voltage/current calibration complete, 0xFF otherwise
 * - zero_calibrated: 0xCA if zero-point calibration complete, 0xFF otherwise
 *
 * Reserved:
 * - 5 bytes reserved for future expansion and structure alignment
 *
 * Calibration Workflow:
 * 1. Measure actual resistor values and update r1/r2/shunt
 * 2. Apply known voltage, measure, calculate voltage_factor
 * 3. Apply known current, measure, calculate current_factor
 * 4. Remove load, measure offsets, store voltage/current_offset
 * 5. Set calibrated flags to 0xCA
 * 6. Write structure to EEPROM for channel
 *
 * @note Structure size: 41 bytes per channel.
 * @note Total storage for 8 channels: 328 bytes + 1 byte CRC = 329 bytes.
 * @note Floats stored in IEEE 754 single-precision format (4 bytes each).
 */
typedef struct {
    /** @ingroup config06 */
    float voltage_factor;    /**< VF calibration factor (default: 0.596). */
    float current_factor;    /**< CF calibration factor (default: 14.871). */
    float voltage_offset;    /**< Zero-point voltage offset in volts. */
    float current_offset;    /**< Zero-point current offset in amps. */
    float r1_actual;         /**< Actual R1 resistor value in ohms (default: 1880000Ω). */
    float r2_actual;         /**< Actual R2 resistor value in ohms (default: 1000Ω). */
    float shunt_actual;      /**< Actual shunt resistor value in ohms (default: 0.001Ω). */
    uint8_t calibrated;      /**< 0xCA if calibrated, 0xFF otherwise. */
    uint8_t zero_calibrated; /**< 0xCA if zero-calibration done, 0xFF otherwise. */
    uint8_t reserved[5];     /**< Reserved bytes for future expansion and alignment. */
} hlw_calib_t;

/**
 * @struct hlw_calib_data_t
 * @brief Container for all 8 channels of HLW8032 calibration data.
 *
 * @details
 * Aggregates calibration records for all 8 power measurement channels into a single
 * structure for bulk EEPROM storage. Includes CRC field for future data integrity
 * validation (currently reserved).
 *
 * Structure:
 * - channels: Array of 8 hlw_calib_t structures (328 bytes total)
 * - crc: Reserved CRC byte for future integrity checking (1 byte)
 *
 * Storage:
 * - Total size: 329 bytes (8 × 41 bytes + 1 byte CRC)
 * - EEPROM location: EEPROM_SENSOR_CAL_START (0x0400)
 * - Region size: 336 bytes allocated (7 bytes spare for expansion)
 *
 * Usage:
 * - Read on boot to populate MeterTask calibration
 * - Written after UART calibration commands
 * - Individual channels can be accessed directly via EEPROM offset
 * - Bulk structure used for backup/restore operations
 *
 * @note Total structure size: 329 bytes.
 * @note CRC field reserved for future use, not currently validated.
 * @note Channel 0 at offset +0, Channel 1 at offset +41, etc.
 */
typedef struct {
    /** @ingroup config06 */
    hlw_calib_t channels[8]; /**< Per-channel calibration records (8 × 41 = 328 bytes). */
    uint8_t crc;             /**< Reserved CRC byte for future use (1 byte). */
} hlw_calib_data_t;

/* =====================  Constants  ==================================== */

/**
 * @name Constants
 * @ingroup config06
 * @{
 */

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

/** @} */

#endif /* EEPROM_MEMORY_MAP_H */

/** @} */