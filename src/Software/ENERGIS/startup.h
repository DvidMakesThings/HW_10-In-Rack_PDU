/**
 * @file startup.h
 * @author David Sipos (DvidMakesThings)
 * @brief Initialization functions for RP2040 GPIOs and peripherals.
 * @version 1.0
 * @date 2025-03-06
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STARTUP_H
#define STARTUP_H

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/resets.h"
#include "hardware/spi.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#include "CONFIG.h"

// Wiznet IoLibrary headers
#include "network/socket.h"
#include "network/w5x00_spi.h"
#include "network/wizchip_conf.h"

// Drivers & utilities
#include "PDU_display.h"
#include "drivers/CAT24C512_driver.h"
#include "drivers/ILI9488_driver.h"
#include "drivers/MCP23017_display_driver.h"
#include "drivers/MCP23017_relay_driver.h"
#include "drivers/button_driver.h"
#include "startup.h"
#include "utils/EEPROM_MemoryMap.h"
#include "utils/helper_functions.h"

// Static IP configuration
extern wiz_NetInfo g_net_info;

/**
 * @brief Initializes RP2040 pins and peripheral buses.
 *
 * Sets up GPIO directions and functions, and initializes the I2C and SPI buses.
 */
bool startup_init(void);

/**
 * @brief Initializes Core 0.
 *
 * Initializes the EEPROM, MCP23017 I/O expanders, and the ILI9488 display.
 * Also initializes the PDU display and updates the status message.
 */
bool core0_init(void);

/**
 * @brief Initializes Core 1.
 *
 * Initializes the W5500 Ethernet controller and sets up the network parameters.
 */
bool core1_init(void);
#endif // STARTUP_H