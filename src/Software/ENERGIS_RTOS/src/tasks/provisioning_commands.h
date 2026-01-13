/**
 * @file src/tasks/provisioning_commands.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup tasks09 9. Provisioning Command Handlers Header
 * @ingroup tasks
 * @brief Factory provisioning commands for device identity configuration via UART.
 * @{
 *
 * @version 1.0.0
 * @date 2025-12-14
 *
 * @details
 * This module provides secure UART-based command handlers for factory provisioning
 * of device identity parameters. Enables single universal firmware to be customized
 * per-device during manufacturing by setting serial number and regulatory region.
 *
 * Architecture:
 * - Token-based unlock mechanism prevents unauthorized provisioning
 * - Time-limited write window (auto-closes after timeout)
 * - UART/USB-CDC only (no network access for security)
 * - Integrates with DeviceIdentity module for persistent storage
 * - Parameters written to EEPROM and survive reboots
 *
 * Provisioning Workflow:
 * 1. Factory operator connects via UART/USB-CDC
 * 2. Issues PROV UNLOCK <token> with secret firmware token
 * 3. Write window opens for limited time (configurable timeout)
 * 4. Operator sets serial number via PROV SET_SN <serial>
 * 5. Operator sets region via PROV SET_REGION <EU|US>
 * 6. Window auto-closes after timeout or manual PROV LOCK
 * 7. Device reboots to apply MAC address and OCP thresholds
 *
 * Available Commands:
 * - PROV UNLOCK <token>      : Authenticate and open write window
 * - PROV LOCK                : Close write window immediately
 * - PROV SET_SN <serial>     : Set device serial number (alphanumeric + hyphen)
 * - PROV SET_REGION <EU|US>  : Set regulatory region (affects current limit)
 * - PROV STATUS              : Display current provisioning state and parameters
 *
 * Serial Number Format:
 * - Maximum length: DEVICE_SN_MAX_LEN characters
 * - Allowed characters: A-Z, a-z, 0-9, hyphen (-)
 * - Validated by DeviceIdentity module before accepting
 * - Used to derive unique MAC address for Ethernet controller
 *
 * Region Configuration:
 * - EU: 10A current limit (IEC/ENEC regulatory compliance)
 * - US: 15A current limit (UL/CSA regulatory compliance)
 * - Affects overcurrent protection thresholds
 * - Requires reboot to reconfigure OCP module
 *
 * Security Model:
 * - Unlock token is compile-time constant embedded in firmware
 * - Token must be known to operator (not discoverable via protocol)
 * - Write window timeout prevents indefinite provisioning access
 * - UART-only access (no remote provisioning via network)
 * - Provisioned values immutable during normal operation
 *
 * Error Handling:
 * - Invalid token: unlock fails, window remains closed
 * - Invalid serial format: rejected with format guidance
 * - Invalid region: rejected with valid options listed
 * - EEPROM errors: reported with diagnostic codes
 * - All operations provide immediate user feedback
 *
 * Integration:
 * - Called by ConsoleTask command dispatcher
 * - Uses DeviceIdentity module for storage and validation
 * - Cooperates with EEPROM mutex for thread-safe writes
 * - Parameters affect NetTask (MAC), OCP (thresholds), displays
 *
 * @note Provisioning should be performed only during manufacturing.
 * @note Changing region after field deployment may violate regulatory compliance.
 * @note Serial number changes affect MAC address and require network reconfiguration.
 * @note Keep unlock token secret; do not expose in documentation or user manuals.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef PROVISIONING_COMMANDS_H
#define PROVISIONING_COMMANDS_H

/**
 * @name Public API
 * @{
 */
/**
 * @brief Main PROV command dispatcher and help provider.
 *
 * Entry point for all provisioning commands from ConsoleTask. Parses the
 * subcommand token and dispatches to appropriate handler function. Provides
 * help text when called without arguments.
 *
 * Supported Subcommands:
 * - UNLOCK <token>      : Unlock provisioning with authentication token
 * - LOCK                : Lock provisioning and close write window
 * - SET_SN <serial>     : Set device serial number
 * - SET_REGION <EU|US>  : Set regulatory region
 * - STATUS              : Display current provisioning state
 *
 * Command Processing:
 * 1. Extract subcommand token from arguments
 * 2. Normalize to uppercase for case-insensitive matching
 * 3. Extract remaining arguments for subcommand handler
 * 4. Dispatch to appropriate handler based on subcommand
 * 5. Provide immediate user feedback for all operations
 *
 * @param[in] args Full argument string after "PROV" command keyword.
 *                 May be NULL or empty to display help text.
 *
 * @note All output goes directly to console via ECHO macro.
 * @note Whitespace and control characters are trimmed before processing.
 * @note Unknown subcommands display error message and suggest help.
 * @note Thread-safe: called from ConsoleTask context only.
 */
/** @ingroup tasks09 */
void cmd_prov(const char *args);

/** @} */

#endif /* PROVISIONING_COMMANDS_H */

/** @} */
/** @} */