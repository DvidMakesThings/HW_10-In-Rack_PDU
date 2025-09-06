# Button Behaviour 

---

## Timing & constants

- **Debounce (`DEBOUNCE_MS`)** – debounce interval for edges.  
- **Post-action guard (`POST_GUARD_MS`)** – guard interval after a valid action.  
- **Long-press detection (`LONGPRESS_DT`)** – SET long-press threshold.  
- **Blink frequency** – 250 ms period (toggle every 250 ms).  
- **Window timeout** – 10 s inactivity closes selection window.  

---

## Selection variables

- `volatile uint8_t selected_row` – current channel index **0..7** (channels **1..8**).  
- Wrap logic:
  - **PLUS (▶)**: **increment** → 1 → … → 8 → 1.  
  - **MINUS (◀)**: **decrement** → 8 → … → 1 → 8.  

---

## Selection Window State Machine

- **Flags and Timers**
  - `s_sel_active` – true if window open.  
  - `s_sel_last_ms` – last activity timestamp.  
  - `s_blink_last_ms` – last blink toggle.  
  - `s_blink_on` – current blink state.  
  - A `repeating_timer` runs every 50 ms.  

### Opening rules
- **PLUS / MINUS**  
  - If window closed → first press opens window, blink current channel (no step).  
  - If window open → step selection (▶ right/increment, ◀ left/decrement), refresh timer.  

- **SET**  
  - **Short press**:  
    - If window closed → first short press opens window, blink current channel, suppress action.  
    - If window open → toggle relay and refresh timer.  
  - **Long press**:  
    - Clears fault.  
    - Never opens or blinks window.  

### Blinking & timeout
- Every 250 ms, `s_blink_on` toggles.  
- `_sel_show_current_only()` updates the LED for the selected channel.  
- If `(now - s_sel_last_ms) ≥ 10 s`, `_sel_window_stop()` clears LEDs and closes the window.  

---

## ISR behaviour (summary)

- **PLUS (▶)**:  
  - If window closed → open window, no step.  
  - If window open → increment channel, refresh timer.  

- **MINUS (◀)**:  
  - If window closed → open window, no step.  
  - If window open → decrement channel, refresh timer.  

- **SET (●)**:  
  - On FALL: latch, arm long-press alarm.  
  - On RISE:  
    - If long fired → long action (clear fault).  
    - Else short:  
      - If window closed → open window, blink, suppress toggle.  
      - If window open → toggle relay, refresh timer.  

---

## Integration Points

- **Drivers used**  
  - `MCP23017_relay_driver` – relay toggling.  
  - `MCP23017_selection_driver` – blinking selection LEDs.  
  - `MCP23017_display_driver` – status LEDs (untouched).  

- **Functions added**  
  - `_sel_window_start()`, `_sel_window_stop()`, `_sel_show_current_only()`.  
  - `_sel_timer_cb()` invoked via `add_repeating_timer_ms()` in `button_driver_init()`.  

- **Non-breaking Guarantee**  
  - Original relay toggling and selection behaviour preserved.  
  - First press from idle only opens the window and blinks.  
  - SET long press never opens or blinks.  

---
