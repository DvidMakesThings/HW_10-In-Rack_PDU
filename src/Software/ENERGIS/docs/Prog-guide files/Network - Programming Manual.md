# Network

---

## Scope and Goals

- Hardware: WIZnet W5500 (10/100Base-T) over SPI at 40 MHz, with dedicated RESET and INT lines.
- Services: HTTP server (TCP/80) and SNMP agent (UDP/161; traps on UDP/162).
- Persistence: Network configuration in EEPROM with CRC validation and first-boot factory defaults.
- Runtime model: Core-split tasks; Core 1 owns networking; Core 0 produces measurement caches and all other functionality.

---

## File Map and Responsibilities

Low-level WIZnet and SPI
- w5x00_spi.c — RP2040 SPI glue for W5500; chip-select handling; burst transfers; registration of byte/burst read/write callbacks; link wait; socket RAM sizing; convenience routines for applying and printing netinfo.
- wizchip_conf.c — WIZchip middleware; ctlwizchip and ctlnetwork control paths; registration of SPI callbacks and critical-section guards; PHY and timeout configuration; interrupt masks.
- w5500.c — Vendor HAL for register and buffer access; chip checks and primitives used by the middleware.
- socket.c — ioLibrary BSD-like sockets on W5500 (TCP/UDP/IPRAW/MACRAW); used by HTTP and SNMP.
- w5x00_gpio_irq.c — Optional INTn wiring and ISR glue if you enable event-driven processing instead of pure polling.
- timer.c — Lightweight periodic timer/timebase utilities; used notably by the SNMP time tick.

Application services and handlers
- http_server.c — Minimal HTTP server on TCP/80 using socket 0; request parsing, dispatch, bounded buffers, immediate re-listen policy.
- status_handler.c — Builds the status JSON (outlet states, cached measurements, temperature, preferences).
- control_handler.c — Parses POST control actions; toggles outlets via high-level relay API with tagging and integrity checks.
- settings_handler.c — GET/POST network configuration and preferences; persistence to EEPROM; applies new netinfo; coordinates reboot when required.
- form_helpers.c — URL decoding and key=value extraction for form-encoded POST bodies.

Runtime orchestration and startup
- ENERGIS_core0_task.c — Sensor polling and cache production (voltage, current, power, uptime, etc.).
- ENERGIS_core1_task.c — Network service loop that polls sockets and drives HTTP and SNMP.
- ENERGIS_startup.c — Top-level bring-up, including W5500 init and applying EEPROM network configuration.

(Additional global configuration: pins and speeds live in CONFIG.h. Treat it as the source of truth for SPI speed = 40 MHz and W5500 pin assignments.)

---

## Hardware Platform

- Controller: WIZnet W5500 hardwired TCP/IP Ethernet controller.
- PHY/MAC: Integrated 10/100Base-TX with auto-negotiation and link detection.
- Interface: SPI0 at 40 MHz in normal operation; CS asserted per transaction; burst transfers used for throughput.
- Pins: SCK, MOSI, MISO, CS, RESET, INT defined in configuration; INT is optional in current build (polling works fine).
- Socket RAM allocation: Optimized for ENERGIS workloads. Typical layout dedicates a large TX/RX pair to socket 0 (HTTP) and smaller buffers to one or two additional sockets (e.g., SNMP). Adjust with care; HTTP payload size and responsiveness depend on sock0 sizing.
- Link/LEDs: Link status is available via PHY status and surfaced to the UI; the front-panel Ethernet LED reflects link and activity.

---

## Initialization Sequence

1) Platform and buses  
System clocks and GPIOs are brought up. SPI0 is initialized for W5500 at 40 MHz, CS idles high, and SPI callbacks for byte/burst I/O are registered with the WIZchip middleware. A critical-section guard is registered to serialize W5500 transactions.

2) WIZchip bring-up  
WIZchip memory allocation per socket is configured. Global interrupt masks and timeouts are set. The init routine blocks until the PHY reports link up (or times out per policy).

3) Network configuration apply  
Network parameters (IP, subnet mask, gateway, DNS, DHCP mode, MAC where applicable) are loaded from EEPROM. CRC-invalid or missing data triggers factory defaults on first boot. The active configuration is applied to the W5500 via netinfo control calls and printed to logs for diagnostics.

4) Service startup  
HTTP server binds and listens on TCP/80 using socket 0 and enters a request loop. SNMP agent opens UDP/161 (agent) and UDP/162 (traps), initializes its timebase, and sends a WarmStart trap on success.

---

## Runtime Architecture

- Core split  
Core 0 is the producer: it polls sensors (HLW8032, temperature, etc.) and maintains caches in SRAM. Core 1 is the consumer: it owns the network stack, sockets, WIZchip SPI, and all networking services.

- Networking loop  
Core 1 runs a single-owner poll loop. In each iteration it steps the HTTP server (socket 0) and the SNMP agent (UDP sockets), services timers, and yields briefly. This single-owner design prevents cross-core contention on SPI and the WIZchip register space.

- Timers and ticks  
A light timer utility provides periodic ticks. SNMP depends on a stable tick (10 ms granularity) for uptime and retransmission timers. Avoid mixing different timebases for MIB objects.

- Optional IRQ path  
If you enable INTn, packet-ready and link-change events can wake the network task; the ISR glue is isolated so you can flip between polling and IRQ without changing the protocol layers.

---

## HTTP Subsystem (socket 0)

Server model  
A single-threaded, blocking server accepts a request on socket 0, parses it into method, URI, headers, and optionally a form body. After responding, it closes and immediately re-listens on socket 0 to avoid TIME_WAIT exhaustion and to keep the loop straightforward.

Endpoints and handlers
- GET /api/status — Returns JSON containing live relay states (queried directly from the expander) and cached measurements (voltage, current, power, power factor, energy, uptime), along with system temperature and preference flags. The split between immediate relay reads and cached sensor values ensures the UI feels instantaneous while sensors update on their own cadence.
- POST /control — Accepts form-encoded or simple key=value bodies for per-channel on/off and group actions. It delegates to a high-level function that updates relays and synchronizes display LEDs via the dual MCP23017 driver. The source is tagged as HTTP for audit/logging.
- GET/POST /settings — Renders network settings and preferences; on POST, it validates and persists changes to EEPROM (with CRC). Depending on what changed, it either updates the W5500 netinfo live or schedules a reboot so sockets come back cleanly with the new parameters.
- Static pages — All other paths are served from embedded templates. The server keeps buffers bounded and avoids dynamic allocation in the hot path.

Operational considerations
- Keep handlers short and non-blocking; long work belongs on Core 0 or deferred queues.
- Respect the buffer limits when adding new endpoints; the bounded design is deliberate for robustness.

---

## SNMP Subsystem

Agent core  
Implements SNMP v1. It parses/encodes PDUs, supports GET, GETNEXT, and SET, and maintains two UDP sockets: agent (161) and trap (162). A time tick updates counters (e.g., sysUpTime) and pacing for retries.

Custom enterprise MIB  
The enterprise branch exposes:
- System information (device name, serial number, firmware version).
- Network parameters (IP address, subnet mask, gateway, DNS, DHCP flag) as read-only objects.
- Outlet state per channel as read/write objects; also aggregate mask and allOn/allOff convenience controls.
- Internal temperature and other status items as read-only.

Set semantics and safety
- Outlet SET operations route through the same high-level API used by HTTP and buttons. This guarantees LED/relay synchronization, proper logging, and dual-driver asymmetry checks.
- Network parameters are intentionally read-only from SNMP in the current design to avoid misconfiguration over management networks; use the Web UI or UART for changes.

Traps  
A WarmStart trap is emitted on successful agent initialization. Additional enterprise traps can be added for fault events (e.g., asymmetry persistent, thermal alarms).

---

## Configuration Persistence (EEPROM)

- Record layout  
Network settings are stored as a structured record with CRC-8. The record includes static or DHCP mode, IP address, subnet mask, default gateway, and DNS. MAC is fixed in hardware or configured elsewhere depending on build.

- First-boot behavior  
A magic-word check at the end of the EEPROM distinguishes first-boot devices. If uninitialized, factory defaults are written atomically and the unit reboots to come up with a valid configuration.

- Normal boot and changes  
On boot, the record is validated and applied. On user change via Web UI or UART, the record is written back with updated CRC. For changes that would disrupt sockets (IP, mask, gateway), a controlled reboot is performed to ensure clean rebind and predictable behavior.

---

## Concurrency and Safety

- Single SPI owner  
Only the Core-1 network task talks to the W5500. The SPI layer still uses a critical section to guard transactions against any accidental concurrent access.

- Memory bounds and back-pressure  
HTTP uses fixed-size request/response buffers and a re-listen policy to stay resilient under malformed or oversized requests. SNMP uses small UDP buffers and validates lengths strictly.

- Relay/LED integrity  
All external control paths (HTTP, SNMP) converge on the same high-level relay API. The dual MCP23017 driver detects and optionally heals asymmetries between relay states and LED indications. Errors are logged and can drive the front-panel error LED.

---

## Fault Handling and Diagnostics

- Link and PHY  
Initialization waits for link up; failures are logged. During runtime, link drops can be surfaced to the UI and logs; handlers attempt to keep listening sockets ready when link returns.

- Socket failures  
Binding/listening errors trigger immediate cleanup and re-listen; the server avoids lingering TIME_WAIT by closing and re-opening socket 0 between requests.

- EEPROM integrity  
CRC-invalid records are ignored; defaults are applied and a warning is logged. First-boot behavior ensures that an empty EEPROM cannot lead to an unusable network configuration.

- Logging  
Consistent macros are used across layers for clear traces of chip identity, link state, netinfo applied, HTTP requests, SNMP operations, and EEPROM actions. Use the network logging level for verbose bring-up diagnostics.

---

## Performance and Tuning

- SPI throughput  
Operate at 40 MHz for efficient W5500 register and buffer access. Ensure board layout and signal integrity meet this speed.

- Socket RAM sizing  
Keep socket 0 at a generous TX/RX size to accommodate HTTP responses. SNMP requires modest buffers. Changing the allocation affects throughput and latency; re-test after modifications.

- Timeouts and retries  
PHY polling, socket timeouts, and SNMP retry intervals are set conservatively to keep the UI responsive without starving other tasks. Adjust only with a clear measurement plan.

- HTTP payloads  
The bounded server model favors many small, fast requests. If you add larger pages or downloads, consider chunking responses or increasing sock0 buffers.

---

## Security Considerations

- SNMP v1 uses community strings in cleartext; restrict access to trusted management networks or upgrade the deployment environment accordingly.
- The HTTP server does not enforce authentication in the base build; place the PDU behind a secured management VLAN or add authentication at a reverse proxy.
- Prefer static IPs and network segmentation for predictability and containment.

---

## Developer Workflow and Extension Points

- Adding HTTP endpoints  
Extend the URI dispatch table in the server and implement a small handler that reads from caches or calls high-level control APIs. Keep handlers short and avoid blocking work.

- Extending the SNMP MIB  
Add OID entries in the enterprise table with explicit getter/setter callbacks. Maintain strict type and length validation and reuse existing high-level control paths.

- Enabling IRQ-driven W5500  
Wire INTn and enable the ISR helper. Ensure the network loop cooperates with event flags and does not assume purely polling semantics.

- Adjusting network configuration flow  
If you add DHCP or alternative configuration sources, keep EEPROM the single source of truth and preserve CRC validation. Prefer reboot after disruptive changes.

---

## Troubleshooting (Developer)

- Cannot reach HTTP  
Verify link LEDs, check that the applied IP is on the same subnet as your PC, and confirm that socket 0 is listening. If EEPROM settings are suspect, reset to defaults via UART or the recovery path.

- SNMP timeouts  
Confirm UDP/161 reachability, community string, and that the agent socket is open. Check that the SNMP time tick is running.

- Link flaps  
Inspect cabling and switch logs. The network loop should survive link drops; if not, verify that re-listen paths are executed after link returns.

- Slow UI  
Check SPI clock is 40 MHz, verify socket 0 RAM sizing, and ensure Core 0’s sensor polling is not saturating the system. Large HTTP responses may need chunking or buffer tuning.

---

## Quick Reference Index

- SPI speed, pins, macros — CONFIG.h  
- W5500 SPI glue and socket RAM sizing — w5x00_spi.c  
- WIZchip control, callbacks, netinfo — wizchip_conf.c  
- W5500 register primitives — w5500.c  
- Sockets API — socket.c  
- Optional INTn wiring — w5x00_gpio_irq.c  
- Timer and periodic tick — timer.c  
- HTTP server — http_server.c  
- Status JSON — status_handler.c  
- Control POSTs — control_handler.c  
- Settings GET/POST and persistence — settings_handler.c  
- SNMP core (agent/traps) — snmp.c  
- Enterprise MIB and callbacks — snmp_custom.c  
- Core 0 producer task — ENERGIS_core0_task.c  
- Core 1 networking task — ENERGIS_core1_task.c  
- System bring-up — ENERGIS_startup.c

---
