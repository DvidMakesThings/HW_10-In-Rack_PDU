/**
 * @file src/snmp/snmp_networkCtrl.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup snmp02 2. SNMP Agent - Network Configuration
 * @ingroup snmp
 * @brief Network configuration exposure via SNMP
 * @{
 *
 * @version 1.1.0
 * @date 2025-11-08
 *
 * @details
 * This module provides SNMP GET callbacks that expose the device's network
 * configuration as read-only OCTET_STRING values. Network parameters are
 * retrieved from persistent storage (EEPROM) via the StorageTask API.
 *
 * Exposed network parameters:
 * - IP address (dotted-decimal notation)
 * - Subnet mask (dotted-decimal notation)
 * - Gateway address (dotted-decimal notation)
 * - DNS server address (dotted-decimal notation)
 * - MAC address (colon-separated hexadecimal)
 *
 * If storage access fails, functions automatically fall back to compiled-in
 * default values to ensure SNMP queries always return valid data.
 *
 * All functions are RTOS-safe and can be called from the SNMP agent context
 * without blocking other tasks.
 *
 * @note These are read-only OIDs; network configuration must be modified via
 *       the web interface or console commands.
 */

#ifndef SNMP_NETWORK_CTRL_H
#define SNMP_NETWORK_CTRL_H

#include "../CONFIG.h"

/** @name Public API
 * @{
 */

/**
 * @brief SNMP getter for device IP address.
 *
 * Retrieves the configured IP address from persistent storage and formats it
 * as a dotted-decimal string (e.g., "192.168.1.100").
 *
 * @param ptr Output buffer (minimum 16 bytes recommended)
 * @param len Pointer to receive string length (bytes written, excluding null)
 *
 * @return None
 *
 * @note Falls back to compiled default if storage read fails.
 */
void get_networkIP(void *ptr, uint8_t *len);

/**
 * @brief SNMP getter for subnet mask.
 *
 * Retrieves the configured subnet mask from persistent storage and formats it
 * as a dotted-decimal string (e.g., "255.255.255.0").
 *
 * @param ptr Output buffer (minimum 16 bytes recommended)
 * @param len Pointer to receive string length (bytes written, excluding null)
 *
 * @return None
 *
 * @note Falls back to compiled default if storage read fails.
 */
void get_networkMask(void *ptr, uint8_t *len);

/**
 * @brief SNMP getter for gateway address.
 *
 * Retrieves the configured gateway address from persistent storage and formats it
 * as a dotted-decimal string (e.g., "192.168.1.1").
 *
 * @param ptr Output buffer (minimum 16 bytes recommended)
 * @param len Pointer to receive string length (bytes written, excluding null)
 *
 * @return None
 *
 * @note Falls back to compiled default if storage read fails.
 */
void get_networkGateway(void *ptr, uint8_t *len);

/**
 * @brief SNMP getter for DNS server address.
 *
 * Retrieves the configured DNS server address from persistent storage and formats it
 * as a dotted-decimal string (e.g., "8.8.8.8").
 *
 * @param ptr Output buffer (minimum 16 bytes recommended)
 * @param len Pointer to receive string length (bytes written, excluding null)
 *
 * @return None
 *
 * @note Falls back to compiled default if storage read fails.
 */
void get_networkDNS(void *ptr, uint8_t *len);

/**
 * @brief SNMP getter for MAC address.
 *
 * Retrieves the configured MAC address from persistent storage and formats it
 * as a colon-separated hexadecimal string (e.g., "02:45:4E:0C:7B:1C").
 *
 * @param ptr Output buffer (minimum 18 bytes recommended)
 * @param len Pointer to receive string length (bytes written, excluding null)
 *
 * @return None
 *
 * @note Falls back to compiled default if storage read fails.
 */
void get_networkMAC(void *ptr, uint8_t *len);

/** @} */

#endif

/** @} */