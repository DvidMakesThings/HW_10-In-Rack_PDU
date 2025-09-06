# HLW8032 Power Monitoring 

---

## Hardware Interface

- **Chip:** HLW8032 single-phase metering IC  
- **Purpose:** Provides real-time measurement of voltage, current, active power, power factor, and energy.  
- **Interface:** UART, fixed at 4800 baud, 8 data bits, even parity, 1 stop bit (8E1).  
- **Data Format:** Each measurement frame is 24 bytes, ending with a checksum.  
- **Deployment:** One HLW8032 is used per channel. Multiplexed UART selection is used so a single MCU UART can read multiple HLW8032 devices.  
- **Polling Strategy:** Only one HLW8032 is active on the bus at a time; a multiplexer selects which chip to query.  

---

## Measurement Capabilities

- **Voltage (V):** Line voltage per channel.  
- **Current (A):** Load current per channel.  
- **Active Power (W):** Real power consumed.  
- **Power Factor (0.0–1.0):** Ratio of active power to apparent power.  
- **Energy (kWh):** Accumulated consumption derived from power integration over time.  
- **Uptime (s):** Total ON duration for each channel, tracked in firmware alongside HLW8032 readings.  

---

## Data Handling

1. **Frame Reception**  
   - HLW8032 continuously outputs 24-byte data frames.  
   - The driver selects one channel, discards the first frame after switching, and validates the next frame by checksum.  

2. **Parsing & Calibration**  
   - Raw HLW8032 registers are converted into engineering units using calibration factors.  
   - Voltage and current are scaled using constants determined during system calibration.  
   - Small offsets (e.g., −0.7 V for voltage) are applied to compensate hardware tolerances.  

3. **Caching**  
   - Each channel has a cache structure holding the most recent voltage, current, power, power factor, energy, and uptime.  
   - Caches are updated in the background by the polling routine.  
   - The rest of the system (Web UI, SNMP, UART reporting) reads from this cache to avoid blocking.  

4. **Polling Schedule**  
   - A round-robin scheme is used: each call to the poll routine advances to the next channel.  
   - Full refresh of all 8 channels typically takes several seconds (≈6 s cycle).  
   - This provides fresh data while keeping CPU load minimal.  

---

## Integration

- **Status Reporting**  
  - `status_handler` builds JSON for the Web API.  
  - Cached HLW8032 values (V, I, P, uptime) are combined with live relay state so the UI reflects both control and measurement.  

- **Control Handler**  
  - Relay ON/OFF state updates uptime counters.  
  - These counters complement HLW8032 readings by showing how long a channel has been energized.  

- **SNMP & UART**  
  - Exposed via higher-level APIs. Cached values are returned on demand without directly reading the HLW8032 at request time.  

---

## Behaviour at Runtime

- **Startup**  
  - All caches are initialized to zero.  
  - First polling cycle establishes baseline measurements.  

- **Normal Operation**  
  - Poller cycles through channels.  
  - Web UI, SNMP, and UART can access cached values instantly.  
  - Relay actions update uptime tracking in parallel.  

- **Error Handling**  
  - If a frame fails checksum, it is discarded.  
  - Next valid frame is used to refresh the cache.  
  - Persistent failures can be logged or flagged in the status JSON.  

---

## Developer Notes

- Each HLW8032 frame is strictly 24 bytes; checksum validation is mandatory.  
- After multiplexer channel change, the first frame is discarded to ensure synchronization.  
- Calibration factors must be tuned at factory stage to align readings with reference instruments.  
- Energy accumulation is implemented in firmware using power integration; HLW8032 raw pulse counts are the basis.  
- Uptime is not provided by HLW8032 but is maintained by the firmware whenever a channel is ON.  
- Polling intervals should balance freshness of data and CPU overhead; typical is one channel every few hundred milliseconds.  
- Cached values provide non-blocking reads for UI and network interfaces — never query the HLW8032 directly in request contexts.  

---
