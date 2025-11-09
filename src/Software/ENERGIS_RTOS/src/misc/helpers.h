/**
 * @file helpers.h
 * @author DvidMakesThings - David Sipos
 * @brief General-purpose helper functions for string processing and utilities
 *
 * @version 1.0.0
 * @date 2025-11-07
 * @details Provides utility functions for URL decoding, form parsing, and
 *          other common string operations used throughout the application.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HELPERS_H
#define HELPERS_H

#include "../CONFIG.h"

/**
 * @brief Extract key=value from URL-encoded body (in-place), returns NULL if missing
 * @param body The URL-encoded body string to search
 * @param key The key to search for
 * @return Pointer to static buffer containing the value, or NULL if not found
 * @note The returned pointer is to a static buffer that will be overwritten
 *       on the next call. The value is not URL-decoded by this function.
 */
char *get_form_value(const char *body, const char *key);

/**
 * @brief Decode '+' to ' ' and "%XX" to char, in-place
 * @param s The string to decode (modified in-place)
 * @return None
 * @note This function modifies the input string in-place, converting URL
 *       encoding to regular characters. '+' becomes space, and %XX sequences
 *       are converted to their corresponding characters.
 */
void urldecode(char *s);

/**
 * @brief Reads the voltage from a specified ADC channel.
 * @param ch The ADC channel to read from.
 * @param len Pointer to store the length of the data read (not used here).
 * @return The voltage as a float.
 */
float get_Voltage(uint8_t ch);

/**
 * @brief Apply StorageTask networkInfo into W5500 and initialize the chip.
 *
 * @param ni Pointer to networkInfo (ip, gw, sn, dns, mac, dhcp mode)
 * @return true on success, false otherwise
 *
 * @details
 * This function maps StorageTask schema to the driver config struct,
 * calls w5500_set_network and then w5500_chip_init. It allows NetTask
 * to configure networking without needing to know the internal type.
 */
bool ethernet_apply_network_from_storage(const networkInfo *ni);

#endif // HELPERS_H
