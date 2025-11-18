/**
 * @file helper_functions.c
 * @author DvidMakesThings - David Sipos
 * @defgroup util2 2. Helper Functions
 * @ingroup utils
 * @brief Helper functions for the ENERGIS PDU project.
 * @{
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "helper_functions.h"

/**
 * @brief Returns the HTML content for a given request.
 * @param request The HTTP request string.
 * @return The HTML content as a string.
 */
const char *get_page_content(const char *request) {
    if (strstr(request, "GET /settings.html"))
        return settings_html;
    if (strstr(request, "GET /user_manual.html"))
        return user_manual_html;
    if (strstr(request, "GET /programming_manual.html"))
        return programming_manual_html;
    if (strstr(request, "GET /help.html"))
        return help_html;
    return control_html; // Default page
}

/**
 * @brief Reads the voltage from a specified ADC channel.
 * @param ch The ADC channel to read from.
 * @param len Pointer to store the length of the data read (not used here).
 * @return The voltage as a float.
 */
float get_Voltage(uint8_t ch) {
    adc_set_clkdiv(96.0f); // slow down ADC clock
    adc_select_input(ch);

    (void)adc_read(); // throwaway sample
    sleep_us(10);     // allow S&H to settle

    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) {
        acc += adc_read();
    }

    float vtap = (acc / 16.0f) * (ADC_VREF / ADC_MAX);
    return vtap * ADC_TOL; // apply +0.5% correction
}

/**
 * @brief Sets the error state on the display.
 * @param error True to indicate an error, false to clear the error.
 * @return None
 */
void setError(bool error) {
    if (error) {
        mcp_display_write_pin(FAULT_LED, 1);
        gpio_put(PROC_LED, 0);
    } else {
        mcp_display_write_pin(FAULT_LED, 0);
        gpio_put(PROC_LED, 1);
    }
}
