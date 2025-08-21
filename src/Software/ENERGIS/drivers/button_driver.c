/**
 * @file button_driver.c
 * @author DvidMakesThings - David Sipos
 * @brief Handles button inputs and actions for the Energis PDU.
 * @version 1.0
 * @date 2025-03-03
 * 
 * @details This module manages the hardware button interactions for the ENERGIS PDU.
 * It processes button press interrupts and controls the display selection and relay toggling
 * based on user input from the physical buttons (PLUS, MINUS, and SET buttons).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "button_driver.h"

/** @brief Timestamp of the last button press in milliseconds since boot */
volatile uint32_t last_press_time = 0;

/** @brief Currently selected row (0-7) corresponding to relay channels 1-8 */
volatile uint8_t selected_row = 0;

/**
 * @brief Handles button press interrupts.
 * 
 * @details This interrupt service routine (ISR) is triggered when any of the 
 * hardware buttons (PLUS, MINUS, SET) are pressed. It performs the following actions:
 * - PLUS button: Move selection up (decrement row index, wrapping around to 7 if at 0)
 * - MINUS button: Move selection down (increment row index, wrapping around to 0 if at 7)
 * - SET button: Toggle the relay state of the currently selected row
 *
 * @param gpio GPIO pin number that triggered the interrupt.
 * @param events Interrupt events (e.g., GPIO_IRQ_EDGE_FALL for falling edge).
 * 
 * @note The function records the timestamp of each button press for potential 
 * debouncing or long-press detection in other parts of the code.
 */
void button_isr(uint gpio, uint32_t events) {
    /** @brief Get current time for debouncing and long-press detection */
    uint32_t now = to_ms_since_boot(get_absolute_time());

    switch (gpio) {
    case BUT_PLUS:
        /** @brief Move selection up, wrap around to bottom if at top */
        selected_row = (selected_row == 0) ? 7 : selected_row - 1;
        PDU_Display_UpdateSelection(selected_row);
        break;

    case BUT_MINUS:
        /** @brief Move selection down, wrap around to top if at bottom */
        selected_row = (selected_row == 7) ? 0 : selected_row + 1;
        PDU_Display_UpdateSelection(selected_row);
        break;

    case BUT_SET:
        /** @brief Toggle relay state of currently selected channel */
        PDU_Display_ToggleRelay(selected_row + 1);
        break;
    }

    /** @brief Update timestamp of most recent button press */
    last_press_time = now;
}

/**
 * @brief Initializes button interrupts.
 * 
 * @details Configures the hardware GPIO pins for the PLUS, MINUS, and SET buttons 
 * with falling-edge interrupt detection. When any of these buttons are pressed 
 * (transition from HIGH to LOW), the button_isr() function will be called.
 * 
 * @note All buttons use the same interrupt service routine (button_isr) but
 * are distinguished within that routine by their GPIO pin numbers.
 */
void button_driver_init(void) {
    /** @brief Register interrupt handler for PLUS button (up navigation) */
    gpio_set_irq_enabled_with_callback(BUT_PLUS, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    
    /** @brief Register interrupt handler for MINUS button (down navigation) */
    gpio_set_irq_enabled_with_callback(BUT_MINUS, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    
    /** @brief Register interrupt handler for SET button (relay toggle) */
    gpio_set_irq_enabled_with_callback(BUT_SET, GPIO_IRQ_EDGE_FALL, true, &button_isr);
}
