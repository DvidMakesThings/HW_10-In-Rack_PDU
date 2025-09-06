# MCP23017 I/O Expanders 

---

## Roles of Each Expander

- **Relay Expander**  
  Controls the **8 relay outputs** that switch the PDU’s sockets.  

- **Display Expander**  
  Controls the **front-panel LEDs**, including channel indicators, power LED, Ethernet LED, and error LED.  

- **Selection Expander**  
  Drives the **row of orange selection LEDs** that blink when a channel is being chosen via the buttons.  

---

## What the User Sees

- When you toggle a channel ON/OFF (via buttons, web, SNMP, or UART), the corresponding relay switches and its **LED indicator updates instantly**.  
- When an **error occurs**, the red error LED lights up.  
- When you press **PLUS/MINUS**, the orange selection LEDs show which channel is currently selected, blinking for 10 seconds.  
- The **power LED** confirms the unit is powered.  
- The **Ethernet LED** lights when a valid network link is active.  

---

## Error Protection

- The system constantly cross-checks the relay and display expanders.  
- If a mismatch (asymmetry) is detected, the guardian logic attempts to heal it.  
- If the mismatch persists, the error LED lights to warn the user.  

---

