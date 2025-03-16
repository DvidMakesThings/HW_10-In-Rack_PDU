#include "button_driver.h"

volatile uint32_t last_press_time = 0;
volatile uint8_t selected_row = 0;

/**
 * @brief Handles button press interrupts.
 */
void button_isr(uint gpio, uint32_t events) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    switch (gpio) {
    case BUT_PLUS:
        selected_row = (selected_row == 0) ? 7 : selected_row - 1;
        PDU_Display_UpdateSelection(selected_row);
        break;

    case BUT_MINUS:
        selected_row = (selected_row == 7) ? 0 : selected_row + 1;
        PDU_Display_UpdateSelection(selected_row);
        break;

    case BUT_SET:
        PDU_Display_ToggleRelay(selected_row + 1);
        break;
    }

    last_press_time = now;
}

/**
 * @brief Initializes button interrupts.
 */
void button_driver_init(void) {
    gpio_set_irq_enabled_with_callback(BUT_PLUS, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    gpio_set_irq_enabled_with_callback(BUT_MINUS, GPIO_IRQ_EDGE_FALL, true, &button_isr);
    gpio_set_irq_enabled_with_callback(BUT_SET, GPIO_IRQ_EDGE_FALL, true, &button_isr);
}
