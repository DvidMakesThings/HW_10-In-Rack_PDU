#ifndef CORE1_TASK_H
#define CORE1_TASK_H

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/resets.h"
#include "pico/multicore.h"
#include <stdio.h>
#include <string.h>

#include "CONFIG.h"
#include "core0_task.h"

// Wiznet IoLibrary headers
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

extern wiz_NetInfo g_net_info;  // Global network config

void core1_task(void);

#endif // CORE1_TASK_H