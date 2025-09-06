# MCP23017 I/O Expanders 

---

## Hardware Setup

- **Relay MCP23017** – on the relay board, wired to 8 relays.  
- **Display MCP23017** – on the display board, wired to front-panel LEDs.  
- **Selection MCP23017** – on the display board, dedicated to the orange selection LEDs.  
- **Bus:** I²C0 (relays) and I²C1 (display, selection).  
- **Reset pins:** Controlled by GPIOs (`MCP_REL_RST`, `MCP_LCD_RST`).  

---

## Driver Layering

- **Relay Driver (`MCP23017_relay_driver.c`)**  
  - Maintains **shadow registers** of OLATA/OLATB to avoid torn writes.  
  - All access is guarded by a **mutex**.  
  - Provides safe pin, register, and mask operations.  

- **Display Driver (`MCP23017_display_driver.c`)**  
  - Identical structure to the relay driver.  
  - Drives LEDs for relay status, power, Ethernet, and error.  

- **Selection Driver (`MCP23017_selection_driver.c`)**  
  - Same principles (mutex + shadows).  
  - Dedicated to the row of selection LEDs.  

- **Dual Driver (`MCP23017_dual_driver.c`)**  
  - Thin wrapper combining relay + display writes.  
  - Ensures **mirrored updates**: both relay state and LED indication always remain in sync.  
  - Provides `set_relay_state_with_tag()` with logging, and guardian logic to auto-heal mismatches.  

---

## Key Features

- **Shadow Latching** – software copies of OLAT ensure atomic, race-free updates.  
- **Mutex Protection** – all I²C transactions serialized to avoid interleaved writes from multiple producers.  
- **Asymmetry Detection** – dual driver compares relay vs display expanders, reports bitmask of mismatched pins.  
- **Guardian Polling** – periodic task that checks for asymmetry and, if enabled, automatically heals it by re-writing.  

---

## Integration

- **Startup (`ENERGIS_startup.c`)**  
  All three MCP23017s are initialized after EEPROM. A LED “show-off” runs (all on, then sequentially off).  

- **Helper Functions (`helper_functions.c`)**  
  `setError()` lights the error LED via display expander.  

- **UART Commands (`uart_command_handler.c`)**  
  `SET_CH` and `GET_CH` go through the dual driver, ensuring relay and LED remain in sync.  

- **Web Control (`control_handler.c`)**  
  Form POSTs call `set_relay_state_with_tag()`. Any mismatches are logged and optionally healed.  

- **Status (`status_handler.c`)**  
  Reports live relay states and LED status to the Web UI.  

---

## Developer Notes

- Always use the **high-level APIs** (`set_relay_state_with_tag`, `mcp_display_write_pin`, `mcp_selection_write_pin`) instead of raw register writes.  
- Never write directly to MCP registers outside the guarded drivers; bypassing shadow + mutex will cause race conditions.  
- When adding new LED indicators, map them in the **display MCP23017** and extend the display driver.  
- The dual driver’s asymmetry mask is the authoritative diagnostic tool for debugging sync issues.  
- Guardian logic is tunable via `DUAL_GUARD_INTERVAL_MS` (polling rate) and `DUAL_GUARD_AUTOHEAL`.  

---
