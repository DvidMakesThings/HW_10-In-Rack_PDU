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


## Test Steps:

1. Reachability and HTTP sanity:
   1.1 Ping baseline IP.
   1.2 GET / and verify status, headers, and server banner.
   1.3 Verify /, /control, /settings, /help pages exist with correct content.
3. Outlet control tests:
   3.1 Read initial state of channel 1 via HTTP and SNMP.
   3.2 For channels 1–8: set ON via HTTP, verify SNMP=1; set OFF via HTTP, verify SNMP=0.
   3.3 Set ALL ON, verify SNMP=1 for all; set ALL OFF, verify SNMP=0 for all.
   3.4 Run concurrency test with 10 parallel togglers for 3–5 seconds; confirm final SNMP states match last commands.
   3.5 If “Save” vs “Apply” exists, confirm outlet state persistence across reboot.
4. Network configuration tests:
   4.1 Read baseline network info via UI or /api/netinfo.
   4.2 Change IP to 192.168.0.72, verify with ping and SNMP, then revert to baseline.
   4.3 Change GW, SN, and DNS individually, verify via UI/SNMP, then revert to baseline.
   4.4 Submit invalid values (e.g., 999.999.1.1, empty, strings) and verify 4xx response with no config change.
   4.5 Confirm only POST mutates state; GET attempts do not.
   4.6 Repeat POST without Referer header to check CSRF handling.
   4.7 If “Save” mode exists, confirm network persistence across reboot.
5. Telemetry tests (if implemented):
   5.1 GET /api/telemetry and verify JSON includes temperature (0–120 °C), uptime, and input voltage.
   5.2 Verify uptime monotonicity and reset after reboot.
   5.3 Request unknown telemetry field or endpoint; verify proper error handling.
6. HTML validation & UX:
   6.1 Submit settings without required fields; expect error.
   6.2 Submit invalid subnet masks; expect error.
   6.3 If hostname configurable, verify it updates SNMP sysName.0 and UI header.
7. Security header checks:
   7.1 Verify Content-Type correctness (HTML vs JSON).
   7.2 Check X-Frame-Options/CSP headers.
   7.3 Send Origin header, verify CORS not wide-open.
8. Error path test: GET unknown path; expect 200.
9. Cross-checks:
   9.1 For every outlet HTTP action, verify with SNMP OIDs.
   9.2 If UART enabled, cross-check with GET\_CH.
10. If reboot endpoint exists, POST reboot, confirm downtime ≤10 s and fresh uptime on return.
11. Stress/soak:
    11.1 Flip random outlets for 5 minutes; every 5 seconds verify SNMP states; log error rate.
    11.2 Concurrently GET /control and /api/netinfo during flips, measure latency.

## Expected Results:

1. Reachability and HTTP sanity:
   1.1 Ping succeeds with 0% loss.
   1.2 Root GET returns 200/304, Content-Type: text/html, and (if present) a valid Server header.
   1.3 All main pages return 200 with non-empty bodies and correct headers.
   1.4 gzip supported if enabled; otherwise absent.
2. Static content integrity:
   2.1 Branding fingerprints (“ENERGIS”, “Network”, “SNMP/UART”) found in correct pages.
   2.2 All referenced links return 200/304; no broken assets.
3. Outlet control tests:
   3.1 HTTP and SNMP states for channel 1 match.
   3.2 Each channel 1–8 responds ON=1 and OFF=0 via SNMP within 500 ms.
   3.3 ALL ON sets all channels to 1; ALL OFF sets all channels to 0.
   3.4 No HTTP 5xx/timeouts; final SNMP states equal last commands; UI stays responsive.
   3.5 Saved outlet pattern restored after reboot.
4. Network configuration tests:
   4.1 Baseline network info matches pass-file.
   4.2 IP change to 192.168.0.72 applies; ping works; SNMP shows new IP; revert restores baseline.
   4.3 GW/SN/DNS updates apply correctly and revert successfully.
   4.4 Invalid values rejected with 4xx; configuration unchanged.
   4.5 GET requests do not change configuration; POST required.
   4.6 POST without Referer is either accepted (no CSRF enforced) or 403 (if enforced); behavior is consistent.
   4.7 Saved settings persist after reboot.
5. Telemetry tests (if implemented):
   5.1 JSON includes temperature, uptime, voltage fields; temperature within \[0–120] °C.
   5.2 Uptime increases over time and resets after reboot.
   5.3 Unknown telemetry request returns 404 / error JSON, not 200.
6. HTML validation & UX:
   6.1 Missing required fields rejected with error; no config change.
   6.2 Invalid subnet masks rejected with error; config unchanged.
   6.3 Hostname set appears in sysName.0 and UI header.
7. Security header checks:
   7.1 HTML returns text/html; JSON returns application/json.
   7.2 Security headers present (X-Frame-Options/CSP).
   7.3 No wildcard Access-Control-Allow-Origin unless explicitly configured.
8. Unknown path returns 200.
9. Cross-checks:
   9.1 All HTTP outlet actions reflected in SNMP OIDs within 500 ms.
   9.2 UART GET\_CH states match HTTP/SNMP.
10. Device reboots on command; ping and HTTP restored ≤15 s; uptime small and increasing.
11. Stress/soak:
    11.1 1-minute random flips complete with 0 HTTP errors; SNMP states consistent; median latency < 500 ms.
    11.2 Concurrent GETs remain <500 ms median; no 5xx.
