/**
 * @file uart_command_handler.h
 * @author David Sipos
 * @brief Header file for UART command handler over stdio (USB-CDC) for ENERGIS PDU.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef UART_COMMAND_HANDLER_H
#define UART_COMMAND_HANDLER_H

#include "CONFIG.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include <string.h>

/**
 * @brief Buffer length for UART command input.
 * @param s The string to trim.
 */
void uart_command_loop(void);

/**
 * @brief Trigger the BOOTSEL mode on the next reboot.
 *
 * This function sets a flag to enter BOOTSEL mode on the next reboot.
 */
void bootsel_trigger(void);

/**
 * @brief Reboot the system.
 *
 * This function reboots the system immediately.
 */
void reboot(void);

/**
 * @brief Dump the EEPROM contents.
 *
 * This function dumps the entire EEPROM contents in a formatted hex table.
 */
void dump_eeprom(void);

/**
 * @brief Set the IP address.
 *
 * This function sets the IP address of the device.
 * @param ip The IP address to set.
 * @param cmd The command string for logging.
 */
void set_ip(const char *ip, const char *cmd);

#endif // UART_COMMAND_HANDLER_H
