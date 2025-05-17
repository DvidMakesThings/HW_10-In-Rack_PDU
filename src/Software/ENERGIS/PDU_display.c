/**
 * @file PDU_display.c
 * @author David Sipos
 * @brief Implements the UI for the PDU display.
 * @version 1.0
 * @date 2025-03-03
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "PDU_display.h"
#include "CONFIG.h"
#include "drivers/ILI9488_driver.h"
#include "drivers/MCP23017_display_driver.h"
#include "drivers/MCP23017_relay_driver.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// Positions for dynamic text fields
#define BASE_Y 50
#define ROW_HEIGHT 30
#define STATE_X 100
#define VOLT_X 170
#define AMP_X 280
#define TEXT_V_X 235
#define TEXT_A_X 345

#define STATUS_X 140
#define IP_X 100
#define CONNECTION_X 300
#define SAVED_X 405

// Text buffer for updates
static char textBuffer[10];

/**
 * @brief Initializes the PDU display.
 */
void PDU_Display_Init(void) {
    // ILI9488_Init();  // Moved to startup.c
    PDU_Display_DrawBackground();
    PDU_Display_DrawStaticUI();
}

/**
 * @brief Draws the background from a BMP file.
 */
void PDU_Display_DrawBackground(void) { ILI9488_FillScreenDMA(COLOR_BLACK); }

/**
 * @brief Draws the static UI elements (table, labels, and lines).
 */
void PDU_Display_DrawStaticUI(void) {
    char label[10];
    uint16_t y;

    // Draw top and bottom bars
    ILI9488_DrawBar(0, 5, ILI9488_WIDTH, 35, COLOR_DARK_BLUE);
    ILI9488_DrawBar(0, 280, ILI9488_WIDTH, 35, COLOR_DARK_BLUE);

    // Draw table lines
    for (uint8_t i = 0; i < 9; i++) {
        y = BASE_Y + (i * ROW_HEIGHT);
        ILI9488_DrawLineDMA(0, y - 9, 400, COLOR_LIGHT_GRAY); // Horizontal lines
    }
    ILI9488_DrawBar(400, 41, 1, 240, COLOR_LIGHT_GRAY); // Vertical line

    PDU_Display_UpdateConnectionStatus(0);

    // Draw channel labels and clear dynamic areas
    for (uint8_t i = 0; i < 8; i++) {
        y = BASE_Y + i * ROW_HEIGHT;

        snprintf(label, sizeof(label), "CH%d:", i + 1);
        ILI9488_DrawText(20, y, label, COLOR_WHITE);

        ILI9488_DrawBar(85, y - 6, 300, 22, COLOR_BLACK);

        PDU_Display_UpdateState(i + 1, "OFF");
        PDU_Display_UpdateVoltage(i + 1, 0.000);
        PDU_Display_UpdateCurrent(i + 1, 0.000);
    }

    // Draw unit labels
    for (uint8_t i = 0; i < 8; i++) {
        y = BASE_Y + i * ROW_HEIGHT;
        ILI9488_DrawText(TEXT_V_X, y, "V", COLOR_WHITE);
        ILI9488_DrawText(TEXT_A_X, y, "A", COLOR_WHITE);
    }

    // Status Bar
    ILI9488_DrawText(20, 295, "STATUS:", COLOR_WHITE);
    PDU_Display_UpdateStatus("OK");

    PDU_Display_UpdateSelection(0); // 0-based index (CH1)
}

/**
 * @brief Updates the ON/OFF state of a given channel.
 * @param channel The channel number (1-8).
 * @param state The state to set (ON or OFF).
 */
void PDU_Display_UpdateState(uint8_t channel, const char *state) {
    if (channel < 1 || channel > 8)
        return;

    uint16_t y = BASE_Y + (channel - 1) * ROW_HEIGHT;

    // Overwrite previous state with background color
    ILI9488_DrawText(STATE_X, y, "OFF ", COLOR_BLACK); // Space ensures full overwrite
    ILI9488_DrawText(STATE_X, y, "ON  ", COLOR_BLACK); // Space ensures full overwrite

    // Draw new state in the correct color
    ILI9488_DrawText(STATE_X, y, state, strcmp(state, "ON") == 0 ? COLOR_GREEN : COLOR_RED);
}

/**
 * @brief Updates the voltage display for a given channel.
 * @param channel The channel number (1-8).
 * @param voltage The voltage value to display.
 */
void PDU_Display_UpdateVoltage(uint8_t channel, float voltage) {
    if (channel < 1 || channel > 8)
        return;
    uint16_t y = BASE_Y + (channel - 1) * ROW_HEIGHT;
    snprintf(textBuffer, sizeof(textBuffer), "%.3f", voltage);
    ILI9488_DrawText(VOLT_X, y, textBuffer, COLOR_WHITE);
}

/**
 * @brief Updates the current display for a given channel.
 * @param channel The channel number (1-8).
 * @param current The current value to display.
 */
void PDU_Display_UpdateCurrent(uint8_t channel, float current) {
    if (channel < 1 || channel > 8)
        return;
    uint16_t y = BASE_Y + (channel - 1) * ROW_HEIGHT;
    snprintf(textBuffer, sizeof(textBuffer), "%.3f", current);
    ILI9488_DrawText(AMP_X, y, textBuffer, COLOR_WHITE);
}

/**
 * @brief Updates the displayed IP address.
 * @param ip The IP address to display (4 bytes).
 */
void PDU_Display_UpdateIP(const uint8_t ip[4]) {
    char ip_str[32]; // Ensure enough space for "IP Address: xxx.xxx.xxx.xxx"
    snprintf(ip_str, sizeof(ip_str), "IP Address: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

    ILI9488_DrawBar(0, 5, 240, 35, COLOR_DARK_BLUE); // Clear IP bar
    ILI9488_DrawText(20, 20, ip_str, COLOR_WHITE);
}

/**
 * @brief Updates the system status message.
 * @param status The status message to display.
 */
void PDU_Display_UpdateStatus(const char *status) {
    ILI9488_DrawBar(0, 280, ILI9488_WIDTH, 35, COLOR_DARK_BLUE); // Clear status bar
    ILI9488_DrawText(STATUS_X, 295, status, COLOR_WHITE);
    ILI9488_DrawText(20, 295, "STATUS:", COLOR_WHITE);
}

/**
 * @brief Shows the connection status.
 * @param connected 1 if connected, 0 otherwise.
 */
void PDU_Display_UpdateConnectionStatus(uint8_t connected) {
    ILI9488_DrawBar(240, 5, ILI9488_WIDTH, 35,
                    COLOR_DARK_BLUE); // Clear IP bar, connection status part
    if (!connected) {
        ILI9488_DrawBar(CONNECTION_X - 15, 10, 130, 25,
                        COLOR_RED); // Background red when not connected
        ILI9488_DrawText(CONNECTION_X, 20, connected ? "CONNECTED" : "NOT CONNECTED",
                         connected ? COLOR_BLACK : COLOR_WHITE);
    } else {
        ILI9488_DrawBar(CONNECTION_X - 15, 10, 130, 30,
                        COLOR_GREEN); // Background red when not connected
        ILI9488_DrawText(CONNECTION_X + 10, 20, connected ? "CONNECTED" : "NOT CONNECTED",
                         connected ? COLOR_BLACK : COLOR_WHITE);
    }
}

/**
 * @brief Displays a "saved" message for 5 seconds when EEPROM save occurs.
 */
void PDU_Display_ShowEEPROM_Saved(void) {
    ILI9488_DrawText(SAVED_X, 295, "saved", COLOR_WHITE);
    sleep_ms(5000);
    ILI9488_DrawBar(SAVED_X, 295, 50, 18, COLOR_DARK_BLUE); // Clear message
}

/**
 * @brief Updates the selection indicator (*) for UI navigation.
 * @param row The row number to highlight (0-based index).
 * @note This function should be called after the static UI is drawn.
 */
void PDU_Display_UpdateSelection(uint8_t row) {
    static uint8_t prev_row = 0;

    // Overwrite previous `*` with background color
    ILI9488_DrawText(5, BASE_Y + prev_row * ROW_HEIGHT, "*", COLOR_BLACK);

    // Draw new selection indicator (*)
    ILI9488_DrawText(5, BASE_Y + row * ROW_HEIGHT, "*", COLOR_WHITE);

    prev_row = row; // Store the current row for the next update
}

/**
 * @brief Toggles the ON/OFF state of the currently selected relay.
 * @param channel The channel number (1-8).
 * @note This function is called when the user presses the "OK" button.
 */
void PDU_Display_ToggleRelay(uint8_t channel) {
    if (channel < 1 || channel > 8)
        return;

    uint16_t y = BASE_Y + (channel - 1) * ROW_HEIGHT;
    static bool relay_states[8] = {false}; // Stores current relay state

    // Toggle relay state
    relay_states[channel - 1] = !relay_states[channel - 1];
    mcp_relay_write_pin(channel - 1, relay_states[channel - 1]); // Set relay

    // Toggle LED indicator on display board
    mcp_display_write_pin(channel - 1, relay_states[channel - 1]);

    // Clear previous state text
    ILI9488_DrawText(STATE_X, y, "OFF ", COLOR_BLACK);
    ILI9488_DrawText(STATE_X, y, "ON  ", COLOR_BLACK);

    // Draw new state text
    ILI9488_DrawText(STATE_X, y, relay_states[channel - 1] ? "ON" : "OFF",
                     relay_states[channel - 1] ? COLOR_GREEN : COLOR_RED);
}
