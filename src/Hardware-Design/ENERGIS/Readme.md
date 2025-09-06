# HLW8032 Upper-Bank Routing Issue — Fixed in HW 1.0.1

## Summary
On **hardware rev 1.0.0**, the measurement path for channels **5–8** is cross-routed at the optocouplers, mirroring the upper nibble. Relays/buttons are unaffected; only measurement presentation would be wrong without compensation.

## Symptom (on HW 1.0.0)
- Turning **CH5–CH8** on shows their voltage/current under the wrong rows in the UI.

| Logical Channel | Routed To (HW 1.0.0) |
|---:|:---|
| 5 | 8 |
| 6 | 7 |
| 7 | 6 |
| 8 | 5 |

## Root Cause
- PCB routing in the optocoupler stage crosses TX lines for channels 5–8.

## Software Handling
- The firmware checks board revision **at compile time** (`SW_REV == 100`) and applies a **MUX-select compensation** only for HW 1.0.0 (invert A/B when the C bit is set).
- Cache indices and UI mapping remain **identity**.
- **Result:** No user-visible issue on any build.

## Hardware Status
- **HW 1.0.1:** Routing corrected (1:1). The firmware automatically **disables** compensation on ≥1.0.1.
- **HW 1.0.0:** Fully functional due to firmware compensation.

## Verification
1. Switch on CH5…CH8 individually.  
2. Confirm the UI shows the correct channel voltage/current on both HW 1.0.0 (with compensation) and HW 1.0.1 (native).

---
**Status:** Fixed in hardware (rev 1.0.1). Software safeguard ensures no impact on users.
