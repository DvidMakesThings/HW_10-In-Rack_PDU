/**
 * @file uart_command_handler.h
 * @author DvidMakesThings - David Sipos
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

/**
 * @brief Set the subnet mask of the device.
 *
 * This function sets the subnet mask of the device and reconfigures the Wiznet chip.
 * @param mask The new subnet mask in dotted-decimal format.
 * @param cmd The command string containing the subnet mask.
 */
void set_subnet(const char *mask, const char *cmd);

/**
 * @brief Set the gateway address of the device.
 * This function sets the gateway address of the device and reconfigures the Wiznet chip.
 * @param gw The new gateway address in dotted-decimal format.
 * @param cmd The command string containing the gateway address.
 */
void set_gateway(const char *gw, const char *cmd);

/**
 * @brief Set the DNS server address of the device.
 * This function sets the DNS server address of the device and reconfigures the Wiznet chip.
 * @param dns The new DNS server address in dotted-decimal format.
 * @param cmd The command string containing the DNS server address.
 */
void set_dns(const char *dns, const char *cmd);

/**
 * @brief Set the network configuration of the device.
 * This function sets the network configuration including IP, subnet, gateway, and DNS.
 * @param full The full command string.
 * @param cmd The command string containing the network configuration.
 */
void set_network(const char *full, const char *cmd);

/**
 * @brief Get system information.
 *
 * This function retrieves and prints system information such as clock frequencies and voltage.
 */
void getSysInfo(void);

/**
 * @brief Test and print values from HLW8032 channels.
 * This function reads the HLW8032 data for all channels and prints the results.
 * This function iterates through all 8 channels and prints the voltage, current, and power
 * readings.
 */
void test_hlw8032_readings(void);

/**
 * @brief Test and print values from HLW8032 channels.
 * This function reads the HLW8032 data for a specific channel and prints the results.
 * @param channel Channel index (1–8) to read from.
 */
void test_hlw8032_read_channel(uint8_t channel);
#endif // UART_COMMAND_HANDLER_H
