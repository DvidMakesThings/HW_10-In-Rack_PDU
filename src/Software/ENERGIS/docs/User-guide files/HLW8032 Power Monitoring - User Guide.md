# HLW8032 Power Monitoring 

---

## What is Measured

- **Voltage (V)** – the mains voltage present on the channel  
- **Current (A)** – the current drawn by the connected load  
- **Power (W)** – real power consumption of the load  
- **Power Factor** – ratio of real power to apparent power (0.0–1.0)  
- **Energy (kWh)** – accumulated energy usage over time  
- **Uptime** – how long a channel has been continuously ON  

---

## Behaviour in Operation

- Measurements are performed **periodically** on all 8 output channels.  
- Values are **cached** so the Web UI and SNMP can read them instantly.  
- Relay **state** is always reported live for instant feedback in the UI.  
- When a channel is turned ON, its uptime counter starts accumulating.  
- When turned OFF, uptime stops until it is switched on again.  
- Energy (kWh) is calculated from pulse counts and accumulated over time.  

---

## User Interaction

- The **Web UI (Status page)** displays live relay states and the most recent voltage, current, and power readings for each channel.  
- Energy consumption and uptime are also visible.  
- SNMP and UART interfaces can report the same values for automation and integration.  
- Users do not need to interact with the HLW8032 chips directly — all readings are managed by the firmware.  
