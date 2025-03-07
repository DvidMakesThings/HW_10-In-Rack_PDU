/**
 * @file PDU_display.c
 * @author David Sipos
 * @brief Implements the UI for the PDU display.
 *        It fills the screen, displays test messages, and draws the static UI elements.
 * @version 1.1
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */

 #include "PDU_display.h"
 #include "drivers/ILI9488_driver.h"
 #include "utils/EEPROM_MemoryMap.h"  // For reading system info from EEPROM
 #include "CONFIG.h"
 #include "pico/stdlib.h"
 #include <stdio.h>
 #include <string.h>
 
 #define SYS_INFO_LEN 31   // Data length stored in System Info region (CRC is extra)
 
 /**
  * @brief Reads the system information from EEPROM and displays it on the top bar.
  */
 static void PDU_Display_ShowSysInfo(void) {
     char sys_info[SYS_INFO_LEN + 1]; // +1 for null terminator
     memset(sys_info, 0, sizeof(sys_info));
     if (EEPROM_ReadSystemInfoWithChecksum((uint8_t *)sys_info, SYS_INFO_LEN) == 0) {
          ILI9488_DrawText(200, 5, sys_info, COLOR_WHITE);
     } else {
          ILI9488_DrawText(200, 5, "SYS INFO ERR", COLOR_RED);
     }
 }
 
 void PDU_Display_DrawStaticUI(void) {
     char label[10];
     int i;
     uint16_t y;
      
     // Draw top blue bar (full width, 25 pixels tall) and display "PDU STATUS"
     ILI9488_DrawBar(0, 0, ILI9488_WIDTH, 25, COLOR_BLUE);
     ILI9488_DrawText(10, 5, "PDU STATUS", COLOR_WHITE);
     PDU_Display_ShowSysInfo();
      
     // Draw bottom blue bar
     ILI9488_DrawBar(0, 295, ILI9488_WIDTH, 25, COLOR_BLUE);
      
     // Draw channel labels and clear dynamic areas for 8 channels.
     for (i = 0; i < 8; i++) {
          y = 35 + i * 32;
          snprintf(label, sizeof(label), "CH%d:", i + 1);
          ILI9488_DrawText(10, y, label, COLOR_WHITE);
          ILI9488_DrawBar(85, y - 6, 300, 22, COLOR_BLACK);
          ILI9488_DrawText(90, y, "ON", COLOR_WHITE);
          ILI9488_DrawText(150, y, "VOLT", COLOR_WHITE);
          ILI9488_DrawText(230, y, "CURR", COLOR_WHITE);
          ILI9488_DrawText(310, y, "PWR", COLOR_WHITE);
      }
 }
  
 void PDU_Display_Test(void) {
     // Initialize display (if not already done in main)
     ILI9488_Init();
     
     // Clear screen to black
     ILI9488_FillScreen(COLOR_BLACK);
     sleep_ms(200);
     
     // Draw test text
     ILI9488_DrawText(50, ILI9488_HEIGHT / 2, "Hello, PDU!", COLOR_WHITE);
     sleep_ms(2000);
     
     // Draw the static UI elements
     PDU_Display_DrawStaticUI();
 }
 