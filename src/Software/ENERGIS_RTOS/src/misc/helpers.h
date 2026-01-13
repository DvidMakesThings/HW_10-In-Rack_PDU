/**
 * @file src/misc/helpers.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup misc2 2. Helpers Module
 * @ingroup misc
 * @brief System-wide helper utilities and diagnostics
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * This module provides general-purpose utility functions and early boot diagnostics
 * that do not fit into specialized modules. It serves as a collection of cross-cutting
 * concerns including URL parsing, network configuration bridging, and boot snapshot
 * capture for reset analysis.
 *
 * Key functionalities:
 * - URL-encoded form data parsing and decoding
 * - Network configuration bridging between storage and driver layers
 * - Early boot reset cause capture and reporting
 * - ADC voltage reading utilities
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HELPERS_H
#define HELPERS_H

#include "../CONFIG.h"

/**
 * @name Public API
 * @ingroup misc2
 * @{
 */
/**
 * @brief Extract value for a given key from URL-encoded form data.
 *
 * Searches a URL-encoded body string for the specified key and returns its
 * associated value in a static buffer. The value is extracted but NOT decoded;
 * caller must call urldecode() separately if needed.
 *
 * This function is commonly used to parse HTTP POST form submissions in the
 * format: key1=value1&key2=value2&...
 *
 * @param body URL-encoded body string to search (must be null-terminated)
 * @param key Key name to search for (must be null-terminated)
 *
 * @return Pointer to static buffer containing the extracted value, or NULL if key not found.
 *         The static buffer is 128 bytes and will be overwritten on the next call.
 *
 * @note This function uses a static buffer; not thread-safe.
 * @note The returned value is not URL-decoded; use urldecode() for full decoding.
 * @note Maximum value length is 127 characters (128th byte is null terminator).
 */
char *get_form_value(const char *body, const char *key);

/**
 * @brief Decode URL-encoded string in-place.
 *
 * Performs in-place URL decoding, converting:
 * - '+' characters to spaces
 * - '%XX' hex sequences to their corresponding bytes
 *
 * The decoded string will always be equal or shorter in length than the
 * original, making in-place modification safe.
 *
 * @param s Null-terminated string to decode (modified in-place)
 *
 * @return None
 *
 * @note The input string is modified in-place.
 * @note Invalid hex sequences are copied as-is without decoding.
 * @note This function is not thread-safe if multiple threads access the same buffer.
 */
void urldecode(char *s);

/**
 * @brief Read voltage from specified ADC channel.
 *
 * Performs an ADC conversion on the specified channel and returns the
 * measured voltage as a floating-point value. Conversion parameters
 * (reference voltage, resolution) are platform-dependent.
 *
 * @param ch ADC channel number to read (platform-specific range)
 *
 * @return Measured voltage as a float value in volts
 *
 * @note ADC must be initialized before calling this function.
 * @note Channel validity is not checked; caller must ensure valid channel number.
 */
float get_Voltage(uint8_t ch);

/**
 * @brief Apply network configuration from storage to W5500 Ethernet controller.
 *
 * This function bridges the storage layer and the W5500 driver layer by translating
 * the networkInfo structure into the driver-specific configuration format and applying
 * it to the hardware. It performs the following operations:
 * - Converts networkInfo to w5500_NetConfig format
 * - Applies configuration to W5500 hardware registers
 * - Initializes the W5500 chip with the new configuration
 * - Briefly flashes the Ethernet LED to indicate successful configuration
 *
 * This abstraction allows NetTask to configure networking without direct knowledge
 * of the driver's internal types or initialization sequence.
 *
 * @param ni Pointer to networkInfo structure containing IP, gateway, subnet, DNS, MAC, and DHCP
 * mode
 *
 * @return true on successful configuration and initialization, false on failure
 *
 * @note The function validates the input pointer before use.
 * @note Failure conditions are logged via the error logging system if enabled.
 */
bool ethernet_apply_network_from_storage(const networkInfo *ni);

/**
 * @brief Log detailed reset cause information at boot.
 *
 * Reads RP2040/RP2350 hardware reset cause registers and outputs comprehensive
 * diagnostics including chip reset flags, watchdog reason, and scratch register
 * contents. This function decodes and reports:
 * - Chip reset flags (POR, RUN, Watchdog, VREG, Temperature)
 * - Watchdog reason register
 * - All 8 watchdog scratch registers
 * - Fault context breadcrumbs if present (0xBEEF signature)
 *
 * If a fault signature is detected in scratch[0], the function decodes the
 * fault type (HardFault, StackOverflow, MallocFail, Assert) and prints the
 * associated context from the remaining scratch registers.
 *
 * @return None
 *
 * @note This function is read-only and does not modify hardware state.
 * @note Platform-specific to RP2040/RP2350; gracefully handles other platforms.
 * @note Call this after logger initialization for diagnostic output.
 */
void Boot_LogResetCause(void);

/**
 * @brief Capture early boot reset snapshot into retained memory.
 *
 * This function must be called as the very first statement in main(), before
 * any subsystem initialization. It captures a minimal diagnostic snapshot into
 * non-initialized RAM that survives resets:
 * - Validates or initializes the snapshot structure
 * - Increments monotonic boot counter
 * - Reads raw reset reason bits from hardware
 * - Copies all 8 watchdog scratch registers
 *
 * The snapshot is designed to be captured before any potential initialization
 * failures that might prevent normal logging. It uses no heap, no peripherals
 * beyond hardware register reads, and minimal stack space.
 *
 * @return None
 *
 * @note MUST be called before clock configuration, RTOS, logger, or USB initialization.
 * @note Uses noinit RAM section; contents survive across soft resets.
 * @note Pair with Helpers_LateBootDumpAndClear() to print the snapshot.
 */
void Helpers_EarlyBootSnapshot(void);

/**
 * @brief Print the early boot snapshot and clear transient diagnostics.
 *
 * This function should be called during initialization after the logger subsystem
 * is ready. It outputs the boot snapshot that was captured by
 * Helpers_EarlyBootSnapshot(), including:
 * - Monotonic boot counter
 * - Raw reset reason bits
 * - All 8 watchdog scratch register values
 *
 * After printing, the function clears only the transient diagnostic fields
 * (reset bits and scratch registers) while preserving the magic, version, and
 * boot counter for correlation across subsequent boots.
 *
 * @return None
 *
 * @note Call this from initialization task after logger is initialized.
 * @note If no valid snapshot exists, a brief notification is printed.
 * @note The boot counter persists and continues incrementing across boots.
 */
void Helpers_LateBootDumpAndClear(void);
/** @} */

#endif // HELPERS_H

/** @} */