/**
 * @file src/tasks/provisioning_commands.h
 * @author DvidMakesThings - David Sipos
 * @defgroup console Console Task
 * @brief Device Provisioning Command Handlers Header
 * @{
 *
 * @defgroup prov_cmds Provisioning Commands
 * @ingroup console
 * @brief UART Commands for Device Identity Provisioning
 * @{
 * @version 1.0.0
 * @date 2025-12-14
 *
 * @details
 * Provides UART command handlers for device provisioning. These commands
 * allow setting the serial number and region (EU/US) on devices with a
 * single universal firmware.
 *
 * Command Syntax:
 * - PROV UNLOCK <token>  - Unlock provisioning with hex token
 * - PROV LOCK            - Lock provisioning (close write window)
 * - PROV SET_SN <serial> - Set device serial number
 * - PROV SET_REGION <EU|US> - Set device region
 * - PROV STATUS          - Show provisioning status
 *
 * Security:
 * - Token is a fixed hex string embedded in firmware
 * - Write window auto-closes after timeout
 * - No network-based provisioning (UART only)
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef PROVISIONING_COMMANDS_H
#define PROVISIONING_COMMANDS_H

/**
 * @brief Handle PROV command group.
 *
 * @details
 * Parses the subcommand and dispatches to appropriate handler:
 * - UNLOCK <token>
 * - LOCK
 * - SET_SN <serial>
 * - SET_REGION <EU|US>
 * - STATUS
 *
 * @param args Command arguments after "PROV" keyword.
 */
void cmd_prov(const char *args);

#endif /* PROVISIONING_COMMANDS_H */

/** @} */
/** @} */