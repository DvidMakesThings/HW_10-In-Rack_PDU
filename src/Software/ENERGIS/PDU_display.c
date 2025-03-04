/**
 * @file PDU_display.c
 * @author David Sipos
 * @brief Implements the UI test procedure for the PDU display.
 *        This module fills the screen with four colors and then draws the static UI elements.
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */
 
 #include "PDU_display.h"
 #include "drivers/ILI9488_driver.h"
 #include "CONFIG.h"
 #include "pico/stdlib.h"
 #include <stdio.h>
 
 void PDU_Display_DrawStaticUI(void) {
     char label[10];
     int i;
     uint16_t y;
     
     // Draw top blue bar (full width, 25 pixels tall) and display "PDU STATUS"
     ILI9488_DrawBar(0, 0, ILI9488_WIDTH, 25, COLOR_BLUE);
     ILI9488_DrawText(10, 5, "PDU STATUS", COLOR_WHITE);
     
     // Draw bottom bar with blue background
     ILI9488_DrawBar(0, 295, ILI9488_WIDTH, 25, COLOR_BLUE);
     
     // For each channel, draw the channel label and placeholders for dynamic values.
     for (i = 0; i < 8; i++) {
         y = 35 + i * 32;
         snprintf(label, sizeof(label), "CH%d:", i + 1);
         ILI9488_DrawText(10, y, label, COLOR_WHITE);
         
         // Clear the dynamic area for this channel (from x=85, height 22 pixels)
         ILI9488_DrawBar(85, y - 6, 300, 22, COLOR_BLACK);
         
         // Draw fixed placeholders for the channel fields.
         ILI9488_DrawText(90, y, "ON", COLOR_WHITE);    // Placeholder for on/off indicator
         ILI9488_DrawText(150, y, "VOLT", COLOR_WHITE);   // Placeholder for voltage
         ILI9488_DrawText(230, y, "CURR", COLOR_WHITE);   // Placeholder for current
         ILI9488_DrawText(310, y, "PWR", COLOR_WHITE);    // Placeholder for power
     }
 }
 
 void PDU_Display_Test(void) {
     // Fill the screen with four different colors in sequence, with a short delay.
     ILI9488_FillScreen(COLOR_RED);
     sleep_ms(500);
     ILI9488_FillScreen(COLOR_GREEN);
     sleep_ms(500);
     ILI9488_FillScreen(COLOR_BLUE);
     sleep_ms(500);
     ILI9488_FillScreen(COLOR_BLACK);
     sleep_ms(500);
     
     // Draw the static UI elements on top.
     PDU_Display_DrawStaticUI();
 }