/**
 * Device Identification Header
 * Auto-generated from template
 * Do not edit manually - this file is generated during the build process
 * 
 * IMPORTANT: This header serves two purposes:
 * 1. Provides the serial number to your code at compile time
 * 2. Allows the device to be identified when in BOOTSEL mode
 */

#ifndef SERIAL_NUMBER_H
#define SERIAL_NUMBER_H

/**
 * Device unique serial number - DO NOT MODIFY THIS FORMAT
 * This specific format allows the device to be identified
 */
#define SERIAL_NUMBER_STR "SN-"
#define SERIAL_NUMBER_NUM "SN-0167663"
#define SERIAL_NUMBER SERIAL_NUMBER_STR SERIAL_NUMBER_NUM

/**
 * Device firmware version - DO NOT MODIFY THIS FORMAT
 * This specific format allows the device to be identified
 */
#define FIRMWARE_VERSION "1.1.0"
#define FIRMWARE_VERSION_LITERAL 110

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
#define ENERGIS_DEFAULT_IP "192.168.0.21"
#define ENERGIS_DEFAULT_SN "255.255.255.0"
#define ENERGIS_DEFAULT_GW "192.168.0.1"
#define ENERGIS_DEFAULT_DNS "8.8.8.8"

#endif /* SERIAL_NUMBER_H */