/**
 * @file button_driver.h
 * @author David Sipos
 * @brief Handles button inputs and actions for the Energis PDU.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */

#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include "../CONFIG.h"
#include "../PDU_display.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#define LONG_PRESS_THRESHOLD 1000 // 1 second

extern volatile uint8_t selected_row;
extern volatile uint32_t last_press_time;

/**
 * @brief Initializes button interrupts.
 */
void button_driver_init(void);

#endif // BUTTON_DRIVER_H
