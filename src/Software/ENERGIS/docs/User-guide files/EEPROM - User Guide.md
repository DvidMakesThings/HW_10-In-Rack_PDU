# The EEPROM 

---

## What is stored in EEPROM?

- **System Information**  
  Device serial number and firmware version.

- **Relay Output States**  
  The ON/OFF status of all output channels is preserved across reboots.

- **Network Configuration**  
  IP address, subnet mask, gateway, DNS, and DHCP setting.

- **Sensor Calibration**  
  Factory calibration values for energy sensors (voltage, current).

- **Event Logs**  
  Operational history and fault events.

- **User Preferences**  
  Device name, location, and preferred temperature unit.

---

## Behaviour in Operation

- **Automatic Restore**  
  On startup, the system reads all relevant settings from EEPROM and applies them.

- **Automatic Save**  
  Any configuration changes (via Web UI, UART, or SNMP) are written to EEPROM with checksums to guarantee integrity.

---

## User Interaction

- Normally invisible: settings simply persist between power cycles.  
- Web UI (Settings page) automatically loads from and saves to EEPROM.  
- UART commands (`SET_IP`, `SET_DNS`, `DUMP_EEPROM`, etc.) interact with EEPROM indirectly.  
- The user does not need to manage memory addresses or low-level details.
