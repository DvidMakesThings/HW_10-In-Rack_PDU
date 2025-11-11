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
#define SERIAL_NUMBER "SN-0167662"

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

#endif /* SERIAL_NUMBER_H */