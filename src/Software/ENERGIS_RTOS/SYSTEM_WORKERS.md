# ENERGIS RTOS system description (polling only, single-owner drivers)

## Roles and ownership

* **SwitchTask** – sole **MCP23017** owner. Controls all MCP devices in the system (relays, LEDs, and indicators). Maintains canonical output state, enforces interlocks, priority, and safe-state logic.
* **ButtonTask** – scans and debounces all front-panel buttons. Emits intent events (e.g., TOGGLE, POWER_SLEEP_REQUEST) to SwitchTask. No direct MCP access.
* **NetTask** – sole **W5500/SPI** owner. Runs SNMP and HTTP services. Aggregates system state, issues control requests to SwitchTask, performs configuration I/O through StorageTask, and reads MeterTask telemetry for remote reporting.
* **ConsoleTask** – UART CLI (polling). Aggregates full system access via commands. Sends control to SwitchTask, configuration requests to StorageTask, and reads metering data from MeterTask.
* **MeterTask** – polls **HLW8032** over UART without interrupts. Computes Vrms, Irms, Watts, Wh, and publishes telemetry for other tasks.
* **StorageTask** – sole **EEPROM** owner. Handles versioned configuration load/save with checksum, provides get/set operations for NetTask and ConsoleTask, and notifies SwitchTask of policy changes.
* **HealthTask** – watchdog, heartbeats, and stack monitoring. Manages fault policy and can order a SAFE_STATE through SwitchTask.
* **LoggerTask** (optional) – single sink for logs. All diagnostic output redirected to the logging queue.

## Concurrency rules

* Each peripheral has exactly one owning task. No other task accesses its driver directly.
* No driver functions are called from ISRs; only SysTick is active.
* Internal mutexes (e.g., `spiMtx`, `eepromMtx`) exist only inside the owning task if the driver requires guarding.
* Busy-waits replaced by `vTaskDelay()` or queue/notify waits.
* Synchronization between tasks is entirely queue-based; no global mutexes.

## IPC and data flows

* **q_switch_req:** all control sources → SwitchTask. Request fields: `{source, op, channel, value, mask, token, timeout_ms, policy_tag}`.
* **q_switch_rsp:** replies `{status, state_bitmap, fault_code, token}`.
* **q_meter_telemetry:** MeterTask publishes computed samples. NetTask and ConsoleTask read nonblocking.
* **q_storage_req / q_storage_rsp:** ConsoleTask and NetTask communicate with StorageTask.
* **q_log:** central logging stream consumed by LoggerTask.
* **Task notifications:** optional nudges (link change, fault update, etc.).

## SwitchTask behavior

* **State machine:** `INIT → READY → FAULT_SAFE`. On entering READY, applies safe mask or restored configuration.
* **Priority policy:** `Health > Console > Net > Button > Schedule`. Lower-priority requests deferred or denied if conflicts arise.
* **Interlocks:** per-channel deny lists, mutual-exclusion groups, minimum toggle intervals, brown-out guards, and maximum simultaneous toggles.
* **Actions:** write outputs to all MCP23017 devices, update `state_bitmap`, log events, and reply to callers.
* **Safe-state:** apply `ALL_OFF` or configured `SAFE_MASK` from StorageTask.

## Typical flows

* **Button press:** ButtonTask debounce → `{TOGGLE, ch}` → `q_switch_req` → SwitchTask applies output change and updates LEDs → publishes state.
* **Power button:** ButtonTask emits `POWER_SLEEP_REQUEST`; handled by HealthTask/SwitchTask (behavior TBD).
* **CLI command:** ConsoleTask parses → sends request to SwitchTask or StorageTask → waits for reply → prints outcome.
* **SNMP/HTTP request:** NetTask decodes → issues requests to SwitchTask (control) or StorageTask (configuration) → serves MeterTask telemetry.
* **Metering:** MeterTask polls HLW8032 at 20–50 Hz, computes averages, and publishes telemetry. NetTask and ConsoleTask display or transmit latest values.
* **Configuration:** ConsoleTask/NetTask → StorageTask → EEPROM read/write → acknowledgment → SwitchTask reloads policy if necessary.
* **Health:** periodic checks; persistent fault → `SAFE_STATE` command to SwitchTask, system announcement over NetTask and ConsoleTask.

## Timing and priorities (RP2040 @ 125 MHz, 1 kHz tick)

* **Suggested priorities:** Health 22, Net 21, Switch 20, Meter 19, Console 18, Button 17, Storage 16, Logger 10.
* **ButtonTask:** 5–10 ms scan, 2–3 sample debounce.
* **MeterTask:** 20–50 Hz polling, 1 s rolling average.
* **NetTask:** event-driven, wakes on RX/timer.
* **StorageTask:** executes synchronously per request.
* **SwitchTask:** immediate processing, minimum 50–100 ms toggle spacing per channel.

## Configuration keys (EEPROM, versioned + checksum)

* Network: MAC, IP, mask, gateway, SNMP communities.
* Switch policy: `SAFE_MASK`, `restore_on_boot`, interlock groups, `min_toggle_ms`, priority overrides.
* Button map: per-button action table, long-press thresholds, group masks, power-button entry.
* Metering: sample rate, calibration constants, averaging.
* Logging: level, sink enable.
* Health: WDT interval, heartbeat timeouts, fault thresholds.

## Error handling and safety

* MCP23017 communication error increments fault counter; on threshold → `FAULT_SAFE`, apply `SAFE_MASK`, notify HealthTask / NetTask.
* MeterTask timeout → degraded telemetry flag, switching unaffected.
* EEPROM write error → last valid config retained.
* Network congestion → bounded queue backpressure, SwitchTask never blocked.

## Memory and diagnostics

* Monitor heap with `xPortGetFreeHeapSize()` / `xPortGetMinimumEverFreeHeapSize()`.
* Increase `configTOTAL_HEAP_SIZE` only if allocations fail.
* Use queue registry for debugging if necessary.
* Replace all `printf` with log queue events containing `{source, op, channel, duration, status, error_code}`.

## Boot and shutdown

* **Boot:** InitTask creates queues and tasks, requests configuration from StorageTask, delivers policies to SwitchTask. SwitchTask applies `SAFE_MASK`, then restores outputs if allowed. NetTask initializes networking and announces system.
* **Shutdown / fault:** HealthTask issues `SAFE_STATE`; StorageTask flushes data; NetTask publishes final system state.