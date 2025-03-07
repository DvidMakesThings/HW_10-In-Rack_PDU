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

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "config.h"  // Contains pin definitions and bus assignments

/**
 * @brief Initializes RP2040 pins and peripheral buses.
 *
 * Sets up GPIO directions and functions, and initializes the I2C and SPI buses.
 */
void startup_init(void);

#endif // STARTUP_H