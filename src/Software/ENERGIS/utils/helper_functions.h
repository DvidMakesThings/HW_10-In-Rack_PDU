#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include <stdio.h>          // For printf()
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/resets.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>

#include "CONFIG.h"

// Wiznet IoLibrary headers (exactly this order!)
#include "network/wizchip_conf.h"
#include "network/socket.h"
#include "network/w5x00_spi.h"

// Drivers & utilities
#include "drivers/CAT24C512_driver.h"
#include "drivers/ILI9488_driver.h"
#include "drivers/MCP23017_display_driver.h"
#include "drivers/MCP23017_relay_driver.h"
#include "utils/EEPROM_MemoryMap.h"
#include "PDU_display.h"
#include "utils/helper_functions.h"
#include "startup.h"
#include "html/control_html.h"
#include "html/settings_html.h"
#include "html/user_manual_html.h"
#include "html/programming_manual_html.h"
#include "html/help_html.h"

// i2c_scan_bus: Scans an I2C bus and prints detected devices.
void i2c_scan_bus(i2c_inst_t *i2c, const char *bus_name);

//------------------------------------------------------------------------------
// activate_relay: Activates a relay (0-7) and turns on the corresponding LED.
void activate_relay(uint8_t relay_number);

const char *get_page_content(const char *request);

#endif // HELPER_FUNCTIONS_H