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

/**
 * @brief Log the true silicon reset cause at the very start of boot.
 *
 * Reads RP2040 reset-cause hardware and prints a single concise line:
 *   [BOOT] cause: chip=0xXXXXXXXX had{por=?,run=?,wd=?,vreg=?,temp=?} wd_reason=0xXXXXXXXX
 *
 * Place this as the first call in your entry point (before peripheral init).
 * It relies only on RP2040 'hardware/regs' mappings and INFO_PRINT.
 *
 * @note This function is non-intrusive: it does not modify any state bits,
 *       it only reads and prints. All bit fields are also printed raw to
 *       avoid dependence on SDK version or naming differences.
 */
void Boot_LogResetCause(void);

/**
 * @brief Capture a minimal reset snapshot at the very beginning of boot.
 *
 * Persist a monotonic boots counter, raw reset reason bits, and the watchdog
 * scratch registers into noinit memory. Performs no logging and uses no heap.
 *
 * Call this as the first statement in main(), before clocks, USB-CDC, logger,
 * or the RTOS are initialized.
 */
void Helpers_EarlyBootSnapshot(void);

/**
 * @brief Print the previously captured snapshot once logging is available, then clear it.
 *
 * If a valid snapshot is present, prints a short diagnostics block that includes
 * the boots counter, the raw reset bits, and the watchdog scratch registers,
 * then clears the snapshot so the next boot starts fresh.
 *
 * Call this near the end of your deterministic bring-up, after the console/logger is ready.
 */
void Helpers_LateBootDumpAndClear(void);

#endif // HELPERS_H
