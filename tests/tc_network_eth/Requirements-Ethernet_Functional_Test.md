# Test Name:
ENERGIS Ethernet & Web UI Functional Test Suite

## Purpose:
To verify the correct operation of the ENERGIS device Ethernet interface and Web UI, including reachability, static assets,
outlet control, network configuration, telemetry, validation, security headers, error handling, caching, reboot behavior,
and stress testing. The goal is to ensure all web-exposed features work as expected, persist across reboots, and cross-check
against SNMP (and UART if enabled).

## Summary:
This test suite connects to the ENERGIS device via Ethernet/HTTP (using curl) and executes a sequence of checks covering
device reachability, HTTP response correctness, static asset integrity, outlet control (channels 1–8 and ALL ON/OFF),
network configuration changes (IP, GW, SN, DNS) with reboot verification, telemetry endpoints, HTML form validation,
security headers, error path robustness, caching behavior, and reboot handling. Tests include stress/soak scenarios with
concurrent toggles and readers. All state changes are verified against SNMP, and against UART if enabled.

### Notes:
1. HTTP request timeout shall be 3 seconds for single requests; reboot wait window shall be 15 seconds maximum.


# Test Steps 
1. Reachability & HTTP sanity (minimal WebUI pages only)
   1.1 Ping baseline IP
   1.2 GET `/` and verify 200/304 with non-empty body
   1.3 Verify root `Content-Type` is `text/html`
   1.4 GET `/control.html` (200/304, non-empty)
   1.5 GET `/settings.html` (200/304, non-empty)
   1.6 GET `/help.html` (200/304, non-empty)
2. Per-channel ON/OFF + SNMP verify (paced)
   2.1 For channel 1: POST `/control` with `channel1=on` → verify SNMP `CH1=1`
   2.2 For channel 1: POST `/control` with empty form → verify SNMP `CH1=0`
   2.3 Repeat steps 2.1–2.2 for channels 2–8
3. ALL ON/OFF + SNMP verify (paced)
   3.1 POST `/control` with `channel1…8=on` → verify SNMP all=1
   3.2 POST `/control` with empty form → verify SNMP all=0
4. Network configuration change + revert (reboot-aware)
   4.1 POST `/settings` with TEMP\_NEW\_IP + baseline GW/SN/DNS
   4.2 Wait for UART “SYSTEM READY” (new IP)
   4.3 Wait HTTP ready at new IP
   4.4 Verify SNMP NET IP=new
   4.5 POST `/settings` with baseline IP/GW/SN/DNS
   4.6 Wait for UART “SYSTEM READY” (baseline)
   4.7 Wait HTTP ready at baseline IP
   4.8 Verify SNMP NET IP=baseline
5. Error path behavior (branded 200)
   5.1 GET `/this-path-should-not-exist` returns 200 with non-empty body
   5.2 Verify `Content-Type` is `text/html`
6. Finalize (safe state)
   6.1 POST `/control` with empty form
   6.2 Verify SNMP all=0


# Expected Results
1. Reachability & HTTP sanity
   1.1 Ping succeeds
   1.2 Root GET returns 200/304 with non-empty HTML body
   1.3 Root `Content-Type` starts with `text/html`
   1.4 `/control.html` returns 200/304 with non-empty body
   1.5 `/settings.html` returns 200/304 with non-empty body
   1.6 `/help.html` returns 200/304 with non-empty body
2. Per-channel ON/OFF + SNMP verify
   2.1 Channel ON sets SNMP=1
   2.2 Channel OFF sets SNMP=0
   2.3 Relay operations are paced ≥200 ms apart
3. ALL ON/OFF + SNMP verify
   3.1 ALL ON sets all channels=1
   3.2 ALL OFF sets all channels=0
4. Network configuration change + revert
   4.1 POST with TEMP\_NEW\_IP accepted (204/disconnect tolerated)
   4.2 UART “SYSTEM READY” seen after reboot
   4.3 HTTP ready within 12 s at new IP
   4.4 SNMP NET IP matches TEMP\_NEW\_IP
   4.5 Revert POST accepted (204/disconnect tolerated)
   4.6 UART “SYSTEM READY” seen again
   4.7 HTTP ready within 12 s at baseline IP
   4.8 SNMP NET IP matches baseline IP
5. Error path behavior
   5.1 Unknown path returns 200 with branded HTML body
   5.2 `Content-Type` is `text/html`
6. Finalize (safe state)
   6.1 ALL OFF POST successful
   6.2 SNMP reports all channels=0
