# ENERGIS Managed PDU – System Description

## Introduction

The ENERGIS project is a fully featured managed power distribution unit (PDU) for 10‑inch rack deployments.  It provides eight AC outlets that can be switched and monitored individually, a network interface for remote management and telemetry, and a robust firmware based on the RP2040 microcontroller and FreeRTOS.  This document describes the hardware components, system architecture and software design so that designers and developers can understand the PDU’s capabilities and constraints.

## Hardware Architecture

### Core Microcontroller and Peripherals

The PDU is built around a Raspberry Pi Pico board that houses a dual‑core **RP2040** microcontroller.  Only one core is used; it runs at **200 MHz** and FreeRTOS ticks at 1 ms.  The RP2040 exposes multiple peripheral buses.  In ENERGIS these buses are allocated as follows:

* **SPI0 → Ethernet controller:**  The W5500 hard‑wired TCP/IP controller is connected to the RP2040 via SPI0.  All access to SPI is mediated by a mutex because multiple tasks may need to use the W5500.
* **I²C buses:**  The design uses both I²C controllers on the RP2040.  `I2C0` serves the **display board**, which actually contains **two MCP23017 GPIO expanders**.  One expander (sometimes referred to as the “display” MCP) drives the eight per‑channel status LEDs and the three system status LEDs (Power, Network and Fault).  The second expander on the same board drives the eight selection LEDs used to show which outlet is currently selected.  `I2C1` connects the **main board** MCP23017 (driving the eight electromechanical relay coils) and the **CAT24C512 EEPROM**.  This bus arrangement is documented in the initialisation code where I2C0 is configured for “Display + Selection MCP23017s” and I2C1 for the “Relay MCP23017 + EEPROM”【197481366152436†L100-L116】.
* **UART0 → HLW8032 power meter:**  The HLW8032 power‑measurement IC communicates over UART at a fixed 4 800 baud with 8‑N‑1 framing【453483929153108†L78-L85】.  In ENERGIS the power meter is connected to UART0; UART1 is unused.  A GPIO expander selects which channel is being measured.
* **USB‑CDC / UART0 console:**  The firmware provides a console over USB‑CDC by default; a UART0 console at 115 200 baud is also available for debugging.  UART1 is not used.
* **GPIO lines:**  The RP2040 drives several on‑board signals directly (voltage‑regulator enable, W5500 chip‑select and reset, etc.) and monitors four front panel buttons.  The buttons are labelled **Set**, **Up** and **Down**; a fourth button labelled **Power** is reserved but not implemented in firmware.  All other LEDs (eight channel status LEDs, eight selection LEDs and three system status LEDs) are controlled indirectly through the MCP23017 expanders.

### Power Monitoring

Power metering is provided by the HLW8032.  The chip outputs measurement frames over UART at 0.5 Hz.  A GPIO expander and analogue multiplexer allow one HLW8032 channel to measure all eight outlets in sequence.  Measurement characteristics include:

| Parameter                | Value | Source |
|--------------------------|------:|:------|
| Voltage range            | 85–280 V AC | HLW8032 module FAQ【453483929153108†L42-L55】 |
| Current range            | 0.05–10 A (extendable to 20 A) | HLW8032 FAQ【453483929153108†L42-L55】 |
| Sampling frequency       | 0.89475 MHz | HLW8032 FAQ【453483929153108†L42-L55】 |
| Typical measurement error| ±2 % | HLW8032 FAQ【453483929153108†L42-L55】 |
| Baud rate                | Fixed at 4 800 bps, 8‑N‑1 | HLW8032 FAQ【453483929153108†L78-L85】 |

The firmware calibrates the HLW8032 during manufacturing.  Each reading is averaged over multiple samples and converted to voltage (V), current (A), power (W) and accumulated energy (kWh).  A rolling average smooths out noise.  Telemetry is published to other tasks via a FreeRTOS queue.

### Relays and AC Output Stage

The eight outlets are switched by **electromechanical relays**, not solid‑state devices.  Each coil is driven by the main‑board MCP23017 through transistor drivers.  The relay contacts are rated for mains voltage (up to 230 V AC) at currents compatible with the HLW8032 measurement range (typically up to 10–16 A).  The relays default to a safe OFF state on power‑up and are only energised after the SwitchTask applies the user‑configured mask.  Per‑channel LEDs on the display board indicate relay status.

### GPIO Expanders (MCP23017)

Three **MCP23017** I/O expanders augment the RP2040 with 48 additional GPIO lines.  Each MCP23017 has 16 pins split into two 8‑bit ports (Port A and Port B), can be addressed from 0x20 to 0x27 using three address pins【777309287828334†L35-L37】 and can source or sink up to 25 mA per pin【777309287828334†L78-L92】.  They operate from the 3.3 V rail and are connected as follows:

1. **Main board MCP (I²C1)** – Port A drives the eight relay coils through transistor drivers; Port B can monitor additional inputs or drive status lines.  This chip is connected to I2C1 together with the CAT24C512 EEPROM【197481366152436†L100-L116】.
2. **Display board MCP 0 (I²C0)** – Port A drives the eight per‑channel **relay status LEDs**; Port B drives the three **system status LEDs**: green for **Power**, blue for **Network Link** and red for **Fault**.  When a relay is energised the corresponding LED is lit.
3. **Display board MCP 1 (I²C0)** – Port A drives the eight **selection LEDs** used when navigating the UI.  Port B is available for future use.  There is no separate “selection board”; the selection functionality is integrated into this second expander on the display board.

### EEPROM (CAT24C512)

Persistent configuration and energy counters are stored in a CAT24C512 I2C EEPROM.  The chip provides 64 KB organised as 65 536 × 8 bits with 128 byte page writes.  It supports standard (100 kHz), fast (400 kHz) and fast plus (1 MHz) modes and features on chip error correction and write protection via the WP pin【425219584043274†L190-L215】.

### Ethernet Controller (W5500)

Networking is handled by the W5500 hard wired TCP/IP controller.  The W5500 integrates an Ethernet MAC and PHY and provides eight independent sockets, each with hardware support for TCP, UDP, IPv4, ICMP, ARP, IGMP and PPPoE protocols【900467860499312†L11-L62】.  Its features include:

- **32 KB on chip memory** for TX/RX buffers shared across sockets.
- **SPI interface up to 80 MHz** (40 MHz used in ENERGIS)【900467860499312†L11-L62】.
- **Auto negotiating 10/100 Mb/s PHY** with Wake on LAN and power down modes【900467860499312†L11-L62】.
- 3.3 V operation with 5 V tolerant I/O【900467860499312†L11-L62】.

The firmware implements a custom socket API atop the W5500 to avoid vendor libraries and ensures all SPI access is serialized via a mutex.

### Front Panel Interface

The ENERGIS user interface is intentionally simple.  There is **no display**; instead the operator interacts through buttons and LED indicators:

* **Buttons:** The front panel exposes three user‑accessible buttons – **Up**, **Down** and **Set** – plus a fourth **Power** button that is reserved for future use but currently unimplemented.  These buttons are read via GPIO pins and debounced by `ButtonTask`.  `ButtonDrv_InitGPIO()` configures the `BUT_PLUS`, `BUT_MINUS` and `BUT_SET` inputs and leaves the selection LEDs off【178655459596007†L21-L27】.  A short press on the **Set** button toggles the selected relay and mirrors the channel LED, while a long press clears the fault LED【178655459596007†L78-L88】.
* **LED indicators:**  Each outlet has a dedicated status LED on the display board.  A second row of LEDs shows which channel is currently selected for viewing or configuration.  Three separate system LEDs convey overall state: **Power** (green), **Network Link** (blue) and **Fault** (red).  Unlike earlier prototypes, there is no tricolour LED – the three indicators are individual devices.  All LEDs are controlled via the MCP23017 expanders.

Because there is no seven‑segment display, channel numbers or configuration values are communicated via the network interface or the console rather than on the front panel.

### Power Supply

The unit operates from a single AC mains input and derives regulated rails for logic and relays.  A standby supply powers the microcontroller during bootloader mode.  Total consumption is designed to remain below 1 W when idle.  The high power outputs are isolated from logic circuitry.

## Software Architecture

The firmware is built on FreeRTOS v11.0.0 and uses a modular task based design.  Each hardware peripheral has a single owning task.  Inter task communication occurs via queues, event groups and mutexes.

### Task Overview

| Task            | Role and relative priority | Notes |
|-----------------|----------------------------|-------|
| **InitTask**    | Runs at the highest priority during boot | Initialises hardware (GPIO, I²C, SPI, ADC), probes MCP23017s and the EEPROM, and creates all other tasks.  Deletes itself once the system is ready. |
| **LoggerTask**  | High priority | Provides a non‑blocking, thread‑safe logging facility.  Starts first so that other tasks can report status and errors immediately. |
| **ConsoleTask** | High priority | Implements the command‑line interface over USB‑CDC and (optionally) UART0.  Handles commands for status, network configuration and relay control. |
| **StorageTask** | High priority | Owns the EEPROM.  Loads configuration from non‑volatile storage, debounces updates and writes back when values change. |
| **SwitchTask**  | Medium priority (optional) | Drives the main‑board MCP23017 to apply a safe relay state and later to update relay outputs.  This task may be compiled out if relay control is integrated elsewhere. |
| **ButtonTask**  | Medium priority (optional) | Scans the front panel buttons, debounces presses, moves the selection LED and toggles relays.  A long press on Set clears the fault LED【178655459596007†L78-L88】. |
| **NetTask**     | Medium priority | Owns the W5500, runs the HTTP server and SNMP agent, maintains network sockets and monitors link status. |
| **MeterTask**   | Medium priority | Owns the HLW8032.  Polls each channel via the multiplexer, averages the readings and publishes telemetry messages to NetTask and other consumers. |
| **HealthTask**  | Low priority (optional) | Monitors other tasks, feeds the watchdog and reports system health.  Created only after MeterTask is ready. |

Each task exposes an `IsReady()` function so that dependent tasks can wait until initialisation is complete.  Because only one task owns each peripheral, other tasks communicate via queues or event groups rather than touching hardware directly.

### Inter Task Communication

FreeRTOS primitives ensure safe concurrency:

- **Queues** – Telemetry, log messages and network commands are passed between tasks using queues.  For example, MeterTask sends `meter_msg_t` structures to NetTask for SNMP and HTTP publication【997597359911276†L769-L807】.
- **Mutexes** – `i2cMtx`, `spiMtx` and `eepromMtx` protect shared buses【997597359911276†L739-L744】.  Tasks must acquire the relevant mutex with a timeout before accessing a bus; failure to acquire must be handled gracefully.
- **Event Groups** – `system_events` flags indicate when subsystems (logger, storage, networking, metering) are ready.  InitTask waits on these flags before proceeding with boot.
- **Task Notifications** – Interrupt service routines never call blocking functions.  Instead they notify a handler task that executes the required work【997597359911276†L373-L399】.

### FreeRTOS Configuration

The scheduler runs at 1 kHz (1 ms tick) with 24 priority levels.  The kernel uses heap 4 allocation with a static heap defined in `FreeRTOSConfig.h`.  Typical stack sizes range from 1 024 to 4 096 bytes per task, and high water marks are periodically logged to monitor stack usage.  Interrupt priorities are configured so that FreeRTOS API calls can be made from certain interrupts.

### Peripheral Ownership Model

Each peripheral is owned by a single task to eliminate race conditions.  For example, only NetTask touches the W5500; other tasks send commands via a queue【997597359911276†L400-L427】.  Similarly, MeterTask alone calls `hlw8032_poll_once()` and caches results for other tasks【229705003907953†L32-L70】.  Shared buses are protected by mutexes but still have a designated owner task.

### Memory Architecture

Global configuration structures and energy counters reside in RAM.  They are periodically backed up to the EEPROM by StorageTask.  The code segment, FreeRTOS heap and task stacks fit comfortably within the 2 MB flash and 264 KB RAM of the RP2040.  Network buffers are allocated from the kernel heap when sockets are opened.

## Boot Sequence

The bring‑up sequence is deterministic.  The `InitTask` runs at the highest priority to ensure nothing else pre‑empts it during hardware initialisation.  The documented order of operations is as follows【197481366152436†L201-L217】:

1. **Hardware initialisation** – Configure voltage‑regulator control, reset the MCP23017 expanders, set up W5500 chip‑select and reset pins, and initialise I²C, SPI and ADC peripherals【197481366152436†L43-L85】.  The watchdog remains in standby.
2. **Peripheral probing** – Scan the three MCP23017 devices on both I²C buses, verify the EEPROM and check for a sane HLW8032 response.  Report the presence or absence of each device.
3. **Subsystem creation** – Create subsystem tasks in dependency order as specified in the source code【197481366152436†L201-L217】: `LoggerTask` → `StorageTask` → `SwitchTask` (optional) → `ButtonTask` (optional) → `MeterTask` → `NetTask` → `ConsoleTask` → `HealthTask` (optional).  After creating each task, `InitTask` waits until `IsReady()` returns true or a timeout occurs.
4. **Configuration distribution** – Once `StorageTask` signals that configuration has been loaded from EEPROM, its values are sent to other tasks via queues.  If storage fails, defaults are used.
5. **Finalisation** – When all required tasks are ready, system status LEDs are set to indicate “running,” the watchdog timer is enabled and `InitTask` deletes itself.  Normal operation continues under the remaining tasks.

## Data Flows

### Configuration Flow

User initiated changes (via web interface, SNMP or console) are routed to StorageTask.  StorageTask updates the in RAM configuration, debounces rapid changes and writes them to EEPROM.  NetTask and MeterTask subscribe to configuration changes via queues.  This ensures consistent configuration across the system.

### Power Telemetry Flow

MeterTask polls each channel in a round robin fashion, computes voltage, current, power and energy, and sends `meter_msg_t` messages to NetTask.  NetTask updates SNMP OIDs and the HTTP cache, making the data available via the network interface.  ConsoleTask can request the latest reading from MeterTask via a direct function call.

### Network Request Handling Flow

NetTask uses W5500 sockets to serve HTTP and SNMP concurrently.  Incoming HTTP requests are parsed and dispatched to handlers in the `web_handlers` module.  SNMP requests are processed by the `snmp` module, which maps OIDs to getters and setters.  Outgoing packets are enqueued and transmitted via the W5500 driver.  Socket events are polled in a non blocking manner.

## Error Handling and Resilience

The system implements a defensive error handling strategy:

- All functions return status codes and log failures via LoggerTask.  Errors are never ignored.
- Task creation is checked; if a task fails to start, InitTask logs the error and halts the system.
- Peripheral communication uses timeouts to avoid deadlocks.  If a mutex cannot be acquired within a defined period, an error is logged and the operation is abandoned【997597359911276†L1178-L1185】.
- An **Error** LED is illuminated on fatal errors.  The console provides commands to view system status and to reboot.
- Watchdog timers reset the system if the scheduler stalls.

## Performance and Resource Utilisation

The design aims for deterministic behaviour:

- **Task latencies:** Most tasks run at 10–100 ms periods.  Network and logging tasks handle asynchronous events with minimal jitter.  MeterTask maintains a 2 Hz sampling rate per channel.
- **Network throughput:** W5500 hardware offloads TCP/IP, enabling reliable SNMP and HTTP performance even under high traffic.  Up to eight sockets allow simultaneous HTTP connections and SNMP sessions.
- **Power consumption:** Idle power consumption is below 1 W thanks to efficient switching regulators.  Relays add load only when switching.  The W5500 enters power down mode when the link is down.

## Conclusion

The ENERGIS PDU combines a modern microcontroller, robust networking and accurate power metering to provide a dependable and configurable power distribution unit.  Its modular hardware and software design emphasises peripheral ownership, thread safe communication and defensive programming.  Understanding the architecture described above will help engineers develop new features, debug issues and extend the system for future requirements.