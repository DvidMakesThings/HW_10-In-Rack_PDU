/**
 * @file src/serial_number.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup config03 3. Serial Number and Device Identification
 * @ingroup config
 * @brief Serial Number and Device Identification Header
 * @{
 *
 * @version 2.0.0
 * @date 2025-11-08
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
#define SERIAL_NUMBER_NUM "0167662"
#define SERIAL_NUMBER SERIAL_NUMBER_STR SERIAL_NUMBER_NUM

/**
 * Device firmware version - DO NOT MODIFY THIS FORMAT
 * This specific format allows the device to be identified
 */
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_VERSION_LITERAL 100

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
#define ENERGIS_DEFAULT_IP {192, 168, 0, 12}  // Default static IPv4 address octets.
#define ENERGIS_DEFAULT_SN {255, 255, 255, 0} // Default IPv4 subnet mask octets.
#define ENERGIS_DEFAULT_GW {192, 168, 0, 1}   // Default IPv4 gateway octets.
#define ENERGIS_DEFAULT_DNS {8, 8, 8, 8}      // Default IPv4 DNS octets.

#endif /* SERIAL_NUMBER_H */