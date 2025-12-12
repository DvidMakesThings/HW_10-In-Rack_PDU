/**
 * @file src/serial_number.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup config03 3. Serial Number and Device Identification
 * @ingroup config
 * @brief Serial Number and Device Identification Header
 * @{
 *
 * @version 2.1.0
 * @date 2025-12-11
 *
 * @details This header file contains the unique serial number and firmware version
 * for the Energis PDU device. It is auto-generated during the build process
 * from a template to ensure uniqueness and consistency.
 *
 * @note Do not edit manually - this file is generated during the build process
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SERIAL_NUMBER_H
#define SERIAL_NUMBER_H

/**
 * Device unique serial number - DO NOT MODIFY THIS FORMAT
 * This specific format allows the device to be identified
 */
#define SERIAL_NUMBER_STR "SN-"
#define SERIAL_NUMBER_NUM "0167663"
#define SERIAL_NUMBER SERIAL_NUMBER_STR SERIAL_NUMBER_NUM

/**
 * Device firmware version - DO NOT MODIFY THIS FORMAT
 * This specific format allows the device to be identified
 */
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_VERSION_LITERAL 100

/**
 * Device hardware version - DO NOT MODIFY THIS FORMAT
 * This specific format allows the device to be identified
 */
#define HARDWARE_VERSION "1.0.0"
#define HARDWARE_VERSION_LITERAL 100

/**
 * Manufacturing date
 */
#define MANUFACTURING_DATE __DATE__

/**
 * Build timestamp
 */
#define BUILD_TIMESTAMP __DATE__ " " __TIME__

/**
 * ENERGIS default network macros
 */
#define ENERGIS_DEFAULT_IP {192, 168, 0, 22}  // Default static IPv4 address octets.
#define ENERGIS_DEFAULT_SN {255, 255, 255, 0} // Default IPv4 subnet mask octets.
#define ENERGIS_DEFAULT_GW {192, 168, 0, 1}   // Default IPv4 gateway octets.
#define ENERGIS_DEFAULT_DNS {8, 8, 8, 8}      // Default IPv4 DNS octets.

/**
 * @defgroup overcurrent_config Overcurrent Protection Configuration
 * @brief Regional current limit configuration for overcurrent protection.
 * @{
 *
 * ENERGIS location macros determine maximum current limits:
 * - EU Version: 10A maximum (IEC/ENEC compliant)
 * - US Version: 15A maximum (UL/CSA compliant)
 *
 * @note Only ONE version should be set to 1, the other must be 0.
 * @warning Setting both to 1 or both to 0 will cause a compile-time error.
 */

/** @brief Enable EU version (10A limit, IEC/ENEC compliant) */
#define ENERGIS_EU_VERSION 1

/** @brief Enable US version (15A limit, UL/CSA compliant) */
#define ENERGIS_US_VERSION 0

/* Compile-time validation of version selection */
#if (ENERGIS_EU_VERSION + ENERGIS_US_VERSION) != 1
#error "Exactly one of ENERGIS_EU_VERSION or ENERGIS_US_VERSION must be set to 1"
#endif

/**
 * @brief Maximum total current limit in Amperes.
 *
 * @details Derived from regional version selection:
 * - EU: 10.0A
 * - US: 15.0A
 */
#if ENERGIS_EU_VERSION
#define ENERGIS_CURRENT_LIMIT_A 10.0f
#elif ENERGIS_US_VERSION
#define ENERGIS_CURRENT_LIMIT_A 15.0f
#endif

/**
 * @brief Safety threshold margin in Amperes.
 *
 * @details Used to define warning and critical thresholds below the hard limit.
 */
#define ENERGIS_CURRENT_SAFETY_MARGIN_A 0.5f

/**
 * @brief Warning threshold offset in Amperes.
 *
 * @details ERR_SEV_ERROR raised when total current exceeds (LIMIT - 1.0A).
 */
#define ENERGIS_CURRENT_WARNING_OFFSET_A 1.0f

/**
 * @brief Recovery hysteresis offset in Amperes.
 *
 * @details Switch lockout lifted when total current falls below (LIMIT - 2.0A).
 */
#define ENERGIS_CURRENT_RECOVERY_OFFSET_A 1.5f

/** @} */ /* End of overcurrent_config group */

#endif /* SERIAL_NUMBER_H */