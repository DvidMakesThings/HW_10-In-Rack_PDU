/**
 * @file PDU_display.h
 * @author David Sipos
 * @brief 
 * @version 1.0
 * @date 2025-03-03
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 */
 
 #ifndef PDU_DISPLAY_H
 #define PDU_DISPLAY_H
 
 #include <stdint.h>

 #define SYS_INFO_LEN 31
 
 /**
  * @brief Run the display test procedure.
  *
  * This function will fill the screen with four different colors (red, green,
  * blue, then black) and then draw the static UI elements.
  */
 void PDU_Display_Test(void);
 
 /**
  * @brief Draws the static UI elements.
  *
  * Draws a top bar with "PDU STATUS", a bottom status bar, and channel labels
  * for eight channels.
  */
 void PDU_Display_DrawStaticUI(void);
 
 #endif // PDU_DISPLAY_H