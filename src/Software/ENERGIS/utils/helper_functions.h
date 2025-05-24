/**
 * @file helper_functions.h
 * @author DvidMakesThings - David Sipos
 * @brief Header file for helper functions used in the ENERGIS PDU project.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/resets.h"
#include "hardware/spi.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include <stdio.h> // For printf()
#include <stdio.h>
#include <string.h>

#include "CONFIG.h"

// Wiznet IoLibrary headers (exactly this order!)
#include "network/socket.h"
#include "network/w5x00_spi.h"
#include "network/wizchip_conf.h"

// Drivers & utilities
#include "PDU_display.h"
#include "drivers/CAT24C512_driver.h"
#include "drivers/ILI9488_driver.h"
#include "drivers/MCP23017_display_driver.h"
#include "drivers/MCP23017_relay_driver.h"
#include "html/control_html.h"
#include "html/help_html.h"
#include "html/programming_manual_html.h"
#include "html/settings_html.h"
#include "html/user_manual_html.h"
#include "startup.h"
#include "utils/EEPROM_MemoryMap.h"
#include "utils/helper_functions.h"

/**
 * @brief Scans an I2C bus for devices and prints their addresses.
 * @param i2c The I2C instance to scan.
 * @param bus_name The name of the bus for logging purposes.
 */
void i2c_scan_bus(i2c_inst_t *i2c, const char *bus_name);

/**
 * @brief Returns the HTML content for a given request.
 * @param request The HTTP request string.
 * @return The HTML content as a string.
 */
const char *get_page_content(const char *request);

#endif // HELPER_FUNCTIONS_H