/**
 * @file system_startup.c
 * @author DvidMakesThings - David Sipos
 * @brief System hardware initialization implementation (RTOS version)
 *
 * @version 1.0.0
 * @date 2025-11-06
 * @details
 * Initializes all hardware peripherals before RTOS scheduler starts.
 * No tasks, no queues - just raw hardware setup.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SYSTEM_STARTUP_H
#define SYSTEM_STARTUP_H

#include "CONFIG.h"

/**
 * @brief Initialize all hardware peripherals and GPIOs.
 *
 * Sets up:
 * - UART0 pins (for console/logger)
 * - I2C0 and I2C1 buses (for MCPs and EEPROM)
 * - SPI0 (for W5500 Ethernet)
 * - ADC (for voltage/temperature sensing)
 * - GPIO outputs (VREG_EN, PROC_LED, reset pins, W5500_CS)
 * - GPIO inputs (buttons with pull-ups)
 *
 * Must be called BEFORE LoggerTask_Init() and BEFORE scheduler starts.
 *
 * @return true if successful, false on error
 */
bool system_startup_init(void);

#endif /* SYSTEM_STARTUP_H */