# The EEPROM 

---

## Hardware

- **Chip:** CAT24C512, 512 kbit (64 KB) I²C EEPROM  
- **Bus:** I²C1 @ 400 kHz, pins GPIO2 (SDA) / GPIO3 (SCL)  
- **Page Size:** 128 bytes, write-cycle time ≈ 5 ms  
- **Endurance:** 1,000,000 write cycles per cell (typical)  
- **Retention:** 100 years  

---

## Driver Layer (`CAT24C512_driver.c`)

Low-level driver for raw access:

- CAT24C512_Init() – initialize I²C bus and driver  
- CAT24C512_WriteByte(addr, data) / CAT24C512_ReadByte(addr) – single-byte read/write  
- CAT24C512_WriteBuffer(addr, *data, len) / CAT24C512_ReadBuffer(addr, *buf, len) – multi-byte access with page boundary handling  
- CAT24C512_Dump(*buf) – raw dump of full 64 KB into a buffer  
- CAT24C512_DumpFormatted() – UART hex-table dump, framed by EE_DUMP_START / EE_DUMP_END  

⚠️ **Warning:** These functions do not perform checksums or section validation. Use them only for diagnostics or when adding new map sections.

---

## Logical Memory Map (`EEPROM_MemoryMap.c`)

The 64 KB address space is split into fixed sections.  
Each section has a defined start address, size, and integrity mechanism.

| Section              | Start Address | Size       | Purpose                                   | Integrity |
|----------------------|---------------|------------|-------------------------------------------|-----------|
| **System Info**      | 0x0000        | 64 B       | Serial number, firmware version           | CRC-8     |
| **Relay Status**     | 0x0100        | 16 B       | 8 relay outputs (bitmask)                 | none      |
| **Network Settings** | 0x0200        | 24 B + CRC | IP, Subnet, Gateway, DNS, DHCP, MAC       | CRC-8     |
| **Sensor Calibration** | 0x0400      | 1024 B     | Voltage/current calibration values        | none (planned CRC) |
| **Energy Monitoring**| 0x0800        | variable   | Historical energy records, wear-leveling  | none (planned CRC) |
| **Event Logs**       | 0x2000        | variable   | Event/error history, wear-leveling        | none (planned CRC) |
| **User Preferences** | 0x3000        | 65 B + CRC | Device name (32), location (32), unit (1) | CRC-8     |
| **Reserved**         | …             | remaining  | Expansion for future features             | —         |
| **Magic Word**       | 0x7FFE        | 2 B        | Factory defaults check (0xA55A)           | required  |

---

## API Functions

High-level APIs enforce boundaries, integrity, and defaults:

### System Info
- EEPROM_WriteSystemInfo() / EEPROM_ReadSystemInfo()  
- EEPROM_WriteSystemInfoWithChecksum() / EEPROM_ReadSystemInfoWithChecksum()  

### Factory Defaults
- EEPROM_WriteFactoryDefaults() – reset all sections to defaults  
- EEPROM_ReadFactoryDefaults() – verify and restore if invalid  
- EEPROM_EraseAll() – destructive full erase (0xFF everywhere)  
- check_and_init_factory_defaults() – first-boot detection and initialization  

### Relay Output
- EEPROM_WriteUserOutput() / EEPROM_ReadUserOutput()  

### Network Config
- EEPROM_WriteUserNetworkWithChecksum() / EEPROM_ReadUserNetworkWithChecksum()  

### Sensor Calibration *(available but optional)*
- EEPROM_WriteSensorCalibration() / EEPROM_ReadSensorCalibration()  

### Energy Monitoring *(available but optional)*
- EEPROM_AppendEnergyRecord()  

### Event Logs *(available but optional)*
- EEPROM_AppendEventLog()  

### User Preferences
- EEPROM_WriteUserPrefsWithChecksum() / EEPROM_ReadUserPrefsWithChecksum()  
- Loader wrappers: LoadUserPreferences(), LoadUserNetworkConfig()  

---

## Integrity Handling

- **CRC-8** polynomial 0x07, initial value 0x00  
- Applied to: System Info, Network, User Preferences  
- On read:  
  - If CRC mismatch → discard section, load defaults, flag error  
  - User prefs (name/location) enforce ASCII and NUL termination  

---

## Startup Behaviour

On power-up, the EEPROM is checked and initialized:

1. **Magic Word Check**  
   The last word of EEPROM (0x7FFE) stores a magic value 0xA55A.  
   If absent, the device assumes this is **first boot** and writes defaults.

2. **Apply Defaults**  
All sections are written with defaults and valid CRCs.  

3. **Reboot**  
The device reboots after defaults are written.  

4. **Normal Boot**  
On subsequent boots, the magic word prevents re-initialization. Stored settings are loaded.

---

## Integration Points

- **UART (uart_command_handler.c)**  
Commands: SET_IP, SET_DNS, DUMP_EEPROM, RFS, BAADCAFE  

- **Web UI (settings_handler.c, status_handler.c)**  
Network + preferences loaded and saved directly via EEPROM APIs  

- **Control Flow (control_handler.c)**  
Ensures relay states are persisted after changes  

- **Status (status_handler.c)**  
Reports current EEPROM values via Web UI and SNMP  

---

## Developer Notes

- EEPROM writes are **slow** → avoid frequent small writes; batch where possible  
- For new sections:  
- Define start + size in EEPROM_MemoryMap.c  
- Add CRC if user-critical  
- Expose safe APIs in EEPROM_*  
- Avoid raw driver calls (CAT24C512_*) unless debugging  
- Plan wear-leveling for logging sections  
- Always call check_and_init_factory_defaults() at startup  

---
