# ENERGIS SNMP Functional Test

## Purpose
To verify the correct operation of ENERGIS device SNMP commands, including system info,  
network configuration and output control ensuring all features work as  
expected and persist across reboots.

## Summary
This test suite connects to the ENERGIS device via 100 Base ethernet  
and executes a sequence of commands to validate help output, system information, network  
settings (with reboot and verification) and relay output control.  
It checks device responses against a pass-file for correctness and expected behavior.

## Notes
1. Each steps that requires reset by the software shall have 15 secods timeout. The DUT  
   shall open its ports inside the timeout period. The test script shall reconnect to  
   the DUT through serial port and wait for the SYSTEM READY string to continue the tests.
2. Each test step must connect to the serial port within the timeout, and after the test  
   is done, disconnect is needed.

## Test Steps
1. Test Walk the enterprise subtree by calling  
   `snmpwalk -v1 -c public -Ci -Cc 192.168.0.11 1.3.6.1.4.1.19865`
2. Test MIB-II “system” group and verify that the substeps from Step 1 are available  
   individually, by sening  
   2.1 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.2.1.1.1.0`  
   2.2 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.2.1.1.2.0`  
   2.3 `snmpget -v1 -c public 192.168.0.11 1.3.6.0.2.1.1.3.0`  
   2.4 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.2.1.1.4.0`  
   2.5 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.2.1.1.5.0`  
   2.6 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.2.1.1.6.0`  
   2.7 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.2.1.1.7.0`
3. Verify the “Long-length” test entries by sending  
   3.1 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.1.0`  
   3.2 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.0`
4. Verify network configuration by sending  
   4.1 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.4.1.0`  
   4.2 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.4.2.0`  
   4.3 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.4.3.0`  
   4.4 `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.4.4.0`
5. Verify each and every output (N) by:  
   5.1 Checking the status of channel 1 by `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.1.0`  
   5.2 Turning on channel 1 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.1.0 i 1`  
       Verify the status through serial communication by calling `GET_CH 1`  
   5.3 Turning off the channel 1 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.1.0 i 0`  
       Verify the status through serial communication by calling `GET_CH 1`  
   5.4 Checking the status of channel 2 by `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.2.0`  
   5.5 Turning on channel 2 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.2.0 i 1`  
       Verify the status through serial communication by calling `GET_CH 2`  
   5.6 Turning off the channel 2 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.2.0 i 0`  
       Verify the status through serial communication by calling `GET_CH 2`  
   5.7 Checking the status of channel 3 by `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.3.0`  
   5.8 Turning on channel 3 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.3.0 i 1`  
       Verify the status through serial communication by calling `GET_CH 3`  
   5.9 Turning off the channel 3 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.3.0 i 0`  
       Verify the status through serial communication by calling `GET_CH 3`  
   5.10 Checking the status of channel 4 by `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.4.0`  
   5.11 Turning on channel 4 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.4.0 i 1`  
       Verify the status through serial communication by calling `GET_CH 4`  
   5.12 Turning off the channel 4 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.4.0 i 0`  
       Verify the status through serial communication by calling `GET_CH 4`  
   5.13 Checking the status of channel 5 by `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.5.0`  
   5.14 Turning on channel 5 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.5.0 i 1`  
       Verify the status through serial communication by calling `GET_CH 5`  
   5.15 Turning off the channel 5 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.5.0 i 0`  
       Verify the status through serial communication by calling `GET_CH 5`  
   5.16 Checking the status of channel 6 by `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.6.0`  
   5.17 Turning on channel 6 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.6.0 i 1`  
       Verify the status through serial communication by calling `GET_CH 6`  
   5.18 Turning off the channel 6 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.6.0 i 0`  
       Verify the status through serial communication by calling `GET_CH 6`  
   5.19 Checking the status of channel 7 by `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.7.0`  
   5.20 Turning on channel 7 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.7.0 i 1`  
       Verify the status through serial communication by calling `GET_CH 7`  
   5.21 Turning off the channel 7 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.7.0 i 0`  
       Verify the status through serial communication by calling `GET_CH 7`  
   5.22 Checking the status of channel 8 by `snmpget -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.8.0`  
   5.23 Turning on channel 8 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.8.0 i 1`  
       Verify the status through serial communication by calling `GET_CH 8`  
   5.24 Turning off the channel 8 by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.8.0 i 0`  
       Verify the status through serial communication by calling `GET_CH 8`  
   5.25 Turning on ALL channels by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.10.0 i 1`  
       Verify the ON status through serial communication by loop reading `GET_CH x`  
   5.26 Turning off ALL channels by `snmpset -v1 -c public 192.168.0.11 1.3.6.1.4.1.19865.2.9.0 i 1`  
       Verify the OFF status through serial communication by loop reading `GET_CH x`

## Expected Results
1. Succesful Walk the enterprise subtree  
   - `SNMPv2-MIB::sysDescr.0` matches with `"STRING: ENERGIS 8 CHANNEL MANAGED PDU"`  
   - `SNMPv2-MIB::sysObjectID.0` matches with `"OID: SNMPv2-MIB::sysObjectID.0"`  
   - `DISMAN-EVENT-MIB::sysUpTimeInstance "Timeticks: (xxxxx) x:xx:xx.xx"` is present  
   - `SNMPv2-MIB::sysContact.0` matches with `"STRING: dvidmakesthings@gmail.com"`  
   - `SNMPv2-MIB::sysName.0` matches with `"STRING: SN-xxxxxxxxxxxx"`  
   - `SNMPv2-MIB::sysLocation.0` matches with `"STRING: Wien"`  
   - `SNMPv2-MIB::sysServices.0` matches with `"INTEGER: -5"`  
   - `SNMPv2-SMI::enterprises.19865.1.0` matches with `"STRING: "long-length OID Test #1""`

2. Succesful MIB-II “system” group operation  
   2.1 `SNMPv2-MIB::sysDescr.0` matches with `"STRING: ENERGIS 8 CHANNEL MANAGED PDU"`  
   2.2 `SNMPv2-MIB::sysObjectID.0` matches with `"OID: SNMPv2-MIB::sysObjectID.0"`  
   2.3 `DISMAN-EVENT-MIB::sysUpTimeInstance "Timeticks: (xxxxx) x:xx:xx.xx"` is present  
   2.4 `SNMPv2-MIB::sysContact.0` matches with `"STRING: dvidmakesthings@gmail.com"`  
   2.5 `SNMPv2-MIB::sysName.0` matches with `"STRING: SN-xxxxxxxxxxxx"`  
   2.6 `SNMPv2-MIB::sysLocation.0` matches with `"STRING: Wien"`  
   2.7 `SNMPv2-MIB::sysServices.0` matches with `"INTEGER: -5"`

3. Succesful “Long-length” test  
   3.1 Answer matches `"SNMPv2-SMI::enterprises.19865.1.0 = STRING: "long-length OID Test #1""`  
   3.2 Answer fails by  
    ```
    Error in packet
    Reason: (noSuchName) There is no such variable name in this MIB.
    Failed object: SNMPv2-SMI::enterprises.19865.2.0
    ```

4. Succesful network configuration read  
   4.1 Answer matches `"SNMPv2-SMI::enterprises.19865.4.1.0 = STRING: "192.168.0.11"`  
   4.2 Answer matches `"SNMPv2-SMI::enterprises.19865.4.2.0 = STRING: "255.255.255.0"`  
   4.3 Answer matches `"SNMPv2-SMI::enterprises.19865.4.3.0 = STRING: "192.168.0.1"`  
   4.4 Answer matches `"SNMPv2-SMI::enterprises.19865.4.4.0 = STRING: "8.8.8.8"`

5. Verify each and every output (N)  
   5.1 Answer matches `"SNMPv2-SMI::enterprises.19865.2.1.0 = INTEGER: 0"`  
   5.2 Answer matches `"SNMPv2-SMI::enterprises.19865.2.1.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 1"`  
         `[ECHO] CH1: ON`  
   5.3 Answer matches `"SNMPv2-SMI::enterprises.19865.2.1.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 1"`  
         `[ECHO] CH1: OFF`  
   5.4 Answer matches `"SNMPv2-SMI::enterprises.19865.2.2.0 = INTEGER: 0"`  
   5.5 Answer matches `"SNMPv2-SMI::enterprises.19865.2.2.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 2"`  
         `[ECHO] CH2: ON`  
   5.6 Answer matches `"SNMPv2-SMI::enterprises.19865.2.2.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 2"`  
         `[ECHO] CH2: OFF`  
   5.7 Answer matches `"SNMPv2-SMI::enterprises.19865.2.3.0 = INTEGER: 0"`  
   5.8 Answer matches `"SNMPv2-SMI::enterprises.19865.2.3.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 3"`  
         `[ECHO] CH3: ON`  
   5.9 Answer matches `"SNMPv2-SMI::enterprises.19865.2.3.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 3"`  
         `[ECHO] CH3: OFF`  
   5.10 Answer matches `"SNMPv2-SMI::enterprises.19865.2.4.0 = INTEGER: 0"`  
   5.11 Answer matches `"SNMPv2-SMI::enterprises.19865.2.4.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 4"`  
         `[ECHO] CH4: ON`  
   5.12 Answer matches `"SNMPv2-SMI::enterprises.19865.2.4.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 4"`  
         `[ECHO] CH4: OFF`  
   5.13 Answer matches `"SNMPv2-SMI::enterprises.19865.2.5.0 = INTEGER: 0"`  
   5.14 Answer matches `"SNMPv2-SMI::enterprises.19865.2.5.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 5"`  
         `[ECHO] CH5: ON`  
   5.15 Answer matches `"SNMPv2-SMI::enterprises.19865.2.5.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 5"`  
         `[ECHO] CH5: OFF`  
   5.16 Answer matches `"SNMPv2-SMI::enterprises.19865.2.6.0 = INTEGER: 0"`  
   5.17 Answer matches `"SNMPv2-SMI::enterprises.19865.2.6.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 6"`  
         `[ECHO] CH6: ON`  
   5.18 Answer matches `"SNMPv2-SMI::enterprises.19865.2.6.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 6"`  
         `[ECHO] CH6: OFF`  
   5.19 Answer matches `"SNMPv2-SMI::enterprises.19865.2.7.0 = INTEGER: 0"`  
   5.20 Answer matches `"SNMPv2-SMI::enterprises.19865.2.7.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 7"`  
         `[ECHO] CH7: ON`  
   5.21 Answer matches `"SNMPv2-SMI::enterprises.19865.2.7.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 7"`  
         `[ECHO] CH7: OFF`  
   5.22 Answer matches `"SNMPv2-SMI::enterprises.19865.2.8.0 = INTEGER: 0"`  
   5.23 Answer matches `"SNMPv2-SMI::enterprises.19865.2.8.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 8"`  
         `[ECHO] CH8: ON`  
   5.24 Answer matches `"SNMPv2-SMI::enterprises.19865.2.8.0 = INTEGER: 1"`  
         and serial response matches:  
         `[ECHO] Received CMD: "GET_CH 8"`  
         `[ECHO] CH8: OFF`  
   5.25 Answer matches `"SNMPv2-SMI::enterprises.19865.2.10.0 = INTEGER: 1"`  
         and loop reading returns ON status for all channels  
   5.26 Answer matches `"SNMPv2-SMI::enterprises.19865.2.9.0 = INTEGER: 1"`  
         and loop reading returns OFF status for all channels
