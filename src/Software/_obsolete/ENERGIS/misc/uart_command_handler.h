/**
 * @file uart_command_handler.h
 * @brief UART command handler for ENERGIS PDU (10-Inch Rack).
 * @author David Sipos (DvidMakesThings)
 * @date 2025-03-03
 *
 * Implements command parsing and execution over UART (USB-CDC).
 * Provides network, system, and EEPROM control via textual commands.
 *
 * @copyright Copyright (c) 2025 David Sipos
 * @version 1.0
 */

#ifndef UART_COMMAND_HANDLER_H
#define UART_COMMAND_HANDLER_H

#include "CONFIG.h"
#include "hardware/irq.h"
#include "hardware/uart.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include <string.h>

#define UART_CMH_TAG "<UART Command Handler>"

/**
 * @brief Main loop for reading and processing UART commands.
 *
 * Reads characters from UART, assembles lines, and executes commands.
 */
void uart_command_loop(void);

/**
 * @brief Set flag to enter BOOTSEL mode on next reboot.
 *
 * Triggers the bootloader for firmware update via BOOTSEL.
 */
void bootsel_trigger(void);

/**
 * @brief Immediately reboot the system.
 *
 * Performs a software reset using the watchdog.
 */
void reboot(void);

/**
 * @brief Print help information for available commands.
 * @return None
 */
void printHelp(void);

/**
 * @brief Handle UART SET_CH command.
 * @param trimmed The trimmed command string.
 * Parses and executes the SET_CH command to control relay channels.
 * @return None
 */
void handle_uart_set_ch_command(const char *trimmed);

/**
 * @brief Handle UART GET_CH command.
 * @param trimmed The trimmed command string.
 * Parses and executes the GET_CH command to read relay channel states.
 * @return None
 */
void handle_uart_get_ch_command(const char *trimmed);

/**
 * @brief Dump the contents of EEPROM in formatted hex.
 *
 * Reads and prints all EEPROM data for inspection.
 */
void dump_eeprom(void);

/**
 * @brief Set the device IP address and reconfigure network.
 *
 * Updates the IP address, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param ip The new IP address (dotted-decimal string).
 * @param cmd The full command string.
 */
void set_ip(const char *ip, const char *cmd);

/**
 * @brief Set the subnet mask and reconfigure network.
 *
 * Updates the subnet mask, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param mask The new subnet mask (dotted-decimal string).
 * @param cmd The full command string.
 */
void set_subnet(const char *mask, const char *cmd);

/**
 * @brief Set the gateway address and reconfigure network.
 *
 * Updates the gateway, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param gw The new gateway address (dotted-decimal string).
 * @param cmd The full command string.
 */
void set_gateway(const char *gw, const char *cmd);

/**
 * @brief Set the DNS server address and reconfigure network.
 *
 * Updates the DNS address, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param dns The new DNS address (dotted-decimal string).
 * @param cmd The full command string.
 */
void set_dns(const char *dns, const char *cmd);

/**
 * @brief Set full network configuration (IP, subnet, gateway, DNS).
 *
 * Updates all network parameters, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param full_cmd The full command string with parameters.
 * @param cmd The full command string.
 */
void set_network(const char *full_cmd, const char *cmd);

/**
 * @brief Print system information (clock frequencies, voltage).
 *
 * Retrieves and displays system status for diagnostics.
 */
void getSysInfo(void);

/**
 * @brief Read and print HLW8032 sensor values for all channels.
 *
 * Iterates through all 8 HLW8032 channels and prints voltage, current, and power.
 */
void hlw8032_readings(void);

/**
 * @brief Read and print HLW8032 sensor values for a specific channel.
 *
 * Reads voltage, current, and power for the given HLW8032 channel.
 *
 * @param channel Channel index (1–8) to read from.
 */
void hlw8032_read_channel(uint8_t channel);

/**
 * @brief Read and print the die temperature.
 *
 * Reads the die temperature from the ADC and prints it.
 * Also converts the raw ADC value to a temperature in Celsius.
 *
 * @return None
 */
void read_dieTemp(void);

/**
 * @brief Handle HLW8032 calibration command.
 *
 * Performs calibration for a specified channel using known reference
 * voltage and current values.
 *
 * @param args The argument string: "<ch> <voltage> <current>"
 * @return None
 */
void handle_calibrate_command(const char *args);

/**
 * @brief Handle AUTO_CALIBRATE_ZERO command.
 *
 * Performs zero-point offset calibration (0V, 0A) for all 8 channels.
 * All channels should be OFF or disconnected before running this command.
 *
 * @return None
 */
void handle_auto_calibrate_zero(void);

/**
 * @brief Handle AUTO_CALIBRATE_VOLTAGE command.
 *
 * Performs voltage calibration (ref_voltage, 0A) for all 8 channels.
 * All channels should have mains voltage present (no load required).
 *
 * @param ref_voltage The reference voltage (e.g., 230.0)
 * @return None
 */
void handle_auto_calibrate_voltage(float ref_voltage);

#endif // UART_COMMAND_HANDLER_H
