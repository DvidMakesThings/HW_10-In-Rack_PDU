/**
 * @file button_driver.h
 * @author DvidMakesThings - David Sipos
 * @brief Handles button inputs and actions for the Energis PDU.
 * @version 1.0
 * @date 2025-03-03
 * 
 * @details This module provides the interface for hardware button interactions 
 * with the ENERGIS PDU device. It handles button press detection and translates 
 * user input into display navigation and relay control actions.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include "../CONFIG.h"
#include "../PDU_display.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

/** @brief Currently selected row in the display (0-7, corresponding to relay channels 1-8) */
extern volatile uint8_t selected_row;

/** @brief Timestamp of the last button press in milliseconds since boot */
extern volatile uint32_t last_press_time;

/**
 * @brief Initializes button interrupts.
 * 
 * @details Sets up the hardware GPIO pins for the PLUS, MINUS, and SET buttons
 * with falling-edge interrupt detection and registers the button_isr callback.
 * This function must be called during system initialization before button
 * interactions can be processed.
 */
void button_driver_init(void);

#endif // BUTTON_DRIVER_H
