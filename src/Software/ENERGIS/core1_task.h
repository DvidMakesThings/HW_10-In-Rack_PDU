/**
 * @file core1_task.h
 * @author DvidMakesThings - David Sipos
 * @brief Header file for Core 1 task handling SNMP and HTTP server.
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CORE1_TASK_H
#define CORE1_TASK_H

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

extern wiz_NetInfo g_net_info; // Global network config

/**
 * @brief Task for Core 1 to handle SNMP and HTTP server.
 */
void core1_task(void);

#endif // CORE1_TASK_H