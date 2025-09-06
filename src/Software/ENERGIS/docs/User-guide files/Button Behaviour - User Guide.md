# Button Behaviour 

---

## Overview of Buttons

- **PLUS (▶)**  
  Moves the selection **right** (increments the channel number).  
  Wraps from channel **1 → 8**.

- **MINUS (◀)**  
  Moves the selection **left** (decrements the channel number).  
  Wraps from channel **8 → 1**.

- **SET (●)**  
  **Short press:** toggles the currently selected channel ON/OFF.  
  **Long press:** clears the fault indicator.

---

## Selection Window with Blinking LEDs

The ENERGIS PDU includes a **dedicated selection LED row** (second row of orange LEDs).  
These LEDs guide the user during channel selection.

### Idle State
- All selection LEDs are **OFF**.  
- Pressing buttons only updates the main status display LEDs.

### Activating the Selection Window
- The **first press of any button** (PLUS, MINUS, or SET) when idle:
  - Opens a **10-second selection window**.  
  - Starts blinking the LED for the **currently selected channel** at **250 ms on/off**.  
  - Does **not** change channel or toggle any relay yet.  
  - Exception: **SET long press** does not open the window.

### Within the Selection Window
- **PLUS (▶)**  
  - Steps the selection right (increment).  
  - The blinking LED moves to the new channel.  
  - The 10-second timer is reset.  

- **MINUS (◀)**  
  - Steps the selection left (decrement).  
  - The blinking LED moves to the new channel.  
  - The 10-second timer is reset.  

- **SET (short press)**  
  - Toggles the relay of the currently selected channel ON/OFF.  
  - The blinking LED remains on that channel.  
  - The 10-second timer is reset.  

- **SET (long press, hold ≥ 2.5s)**  
  - Clears the error/fault indicator.  
  - Does **not** open or blink the selection window.  

### Window Timeout
- If **no button is pressed for 10 seconds**, the window closes:
  - The blinking LED turns OFF.  
  - The system returns to idle.  

---

## Practical Example
1. From idle, press **PLUS (▶)** → current channel’s LED starts **blinking** (250 ms), no change yet.  
2. Press **PLUS (▶)** again within 10 s → selection moves **right**, blinking follows.  
3. Press **MINUS (◀)** → selection moves **left**, blinking follows.  
4. Press **SET (short)** → toggles that channel; blinking continues.  
5. Wait 10 s → window closes, LEDs turn **OFF**.

---
