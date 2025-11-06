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
#include "ENERGIS_startup.h"
#include "drivers/CAT24C512_driver.h"
#include "html/control_html.h"
#include "html/help_html.h"
#include "html/programming_manual_html.h"
#include "html/settings_html.h"
#include "html/user_manual_html.h"
#include "utils/EEPROM_MemoryMap.h"
#include "utils/helper_functions.h"

/**
 * @brief Returns the HTML content for a given request.
 * @param request The HTTP request string.
 * @return The HTML content as a string.
 */
const char *get_page_content(const char *request);

/**
 * @brief Reads the voltage from a specified ADC channel.
 * @param ch The ADC channel to read from.
 * @param len Pointer to store the length of the data read (not used here).
 * @return The voltage as a float.
 */
float get_Voltage(uint8_t ch);

/**
 * @brief Sets the error state on the display.
 * @param error True to indicate an error, false to clear the error.
 * @return None
 */
void setError(bool error);

#endif // HELPER_FUNCTIONS_H