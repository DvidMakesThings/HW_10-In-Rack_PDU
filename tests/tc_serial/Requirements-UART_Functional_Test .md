# ENERGIS UART Functional Test 

## Purpose
To verify the correct operation of ENERGIS device UART commands, including system info,  
network configuration, output control, and factory reset, ensuring all features work as  
expected and persist across reboots.

## Summary
This test suite connects to the ENERGIS device via UART and executes a sequence of  
commands to validate help output, system information, network settings (with reboot and  
verification), relay output control, and factory reset. It checks device responses  
against a pass-file for correctness and expected behavior.

## Notes
1. Each steps that requires reset by the software shall have 15 secods timeout. The DUT  
   shall open its serial port inside the timeout period. The test script shall reconnect to  
   the DUT through serial port and wait for the SYSTEM READY string to continue the tests.  
2. Each test step must connect to the serial port within the timeout, and after the test is done, disconnect is needed.  

## Test Steps
1. Send HELP and verify the command list matches required tokens.  
2. Send SYSINFO and verify serial, firmware, voltage, and clock parameters.  
3. Network tests:  
   3.1 Read baseline network info (NETINFO).  
   3.2 Change IP address to 192.168.0.72  and verify the change by NETINFO.  
   3.3 Verify the correct functionality by pinging the new IP address  
   3.4 Revert IP address to original and verify it by NETINFO   
   3.5 Change Gateway address to 192.166.0.1 and verify the change by NETINFO.  
   3.6 Revert Gateway address to original and verify it by NETINFO   
   3.7 Change Subnet mask to 255.0.0.0 and verify the change by NETINFO.  
   3.8 Revert Subnet mask to original and verify it by NETINFO   
   3.9 Change DNS to 1.1.1.1 and verify the change by NETINFO.  
   3.10 Revert DNS to original and verify it by NETINFO   
4. Output tests  
   4.1 Set Channel 1 to ON and verify the status by SNMP  
   4.2 Set Channel 1 to OFF and verify the status by SNMP  
   4.3 Set Channel 2 to ON and verify the status by SNMP  
   4.4 Set Channel 2 to OFF and verify the status by SNMP  
   4.5 Set Channel 3 to ON and verify the status by SNMP  
   4.6 Set Channel 3 to OFF and verify the status by SNMP  
   4.7 Set Channel 4 to ON and verify the status by SNMP  
   4.8 Set Channel 4 to OFF and verify the status by SNMP  
   4.9 Set Channel 5 to ON and verify the status by SNMP  
   4.10 Set Channel 5 to OFF and verify the status by SNMP  
   4.11 Set Channel 6 to ON and verify the status by SNMP  
   4.12 Set Channel 6 to OFF and verify the status by SNMP  
   4.13 Set Channel 7 to ON and verify the status by SNMP  
   4.14 Set Channel 7 to OFF and verify the status by SNMP  
   4.15 Set Channel 8 to ON and verify the status by SNMP  
   4.16 Set Channel 8 to OFF and verify the status by SNMP  
5. Test All Channels  
   5.1 Set ALL Channel to ON and verify the status by SNMP  
   5.2 Set ALL Channel to OFF and verify the status by SNMP  
6. Send RFS to reset to factory settings, then verify with SYSINFO.  
7. DUMP the EEPROM content and save it to a file.  
   7.1 Translate the EEPROM content to ASCII  
   7.2 Based on the EEPROM Memory Map verify the data with the translated content  

## Expected Results
1. Output lists all required commands and tokens.  
2. Serial number, firmware version, voltage, and clock parameters are valid and within  
   expected ranges.  
3. Network tests:  
   3.1 NETINFO returns baseline network info.  
   3.2 IP address changes to 192.168.0.72; NETINFO confirms change.  
   3.3 Device responds to ping at 192.168.0.72.  
   3.4 IP address reverts to original; NETINFO confirms revert.  
   3.5 Gateway changes to 192.166.0.1; NETINFO confirms change.  
   3.6 Gateway reverts to original; NETINFO confirms revert.  
   3.7 Subnet mask changes to 255.0.0.0; NETINFO confirms change.  
   3.8 Subnet mask reverts to original; NETINFO confirms revert.  
   3.9 DNS changes to 1.1.1.1; NETINFO confirms change.  
   3.10 DNS reverts to original; NETINFO confirms revert.  
4. Output tests  
   4.1 Channel 1 ON; SNMP verifies status.  
   4.2 Channel 1 OFF; SNMP verifies status.  
   4.3 Channel 2 ON; SNMP verifies status.  
   4.4 Channel 2 OFF; SNMP verifies status.  
   4.5 Channel 3 ON; SNMP verifies status.  
   4.6 Channel 3 OFF; SNMP verifies status.  
   4.7 Channel 4 ON; SNMP verifies status.  
   4.8 Channel 4 OFF; SNMP verifies status.  
   4.9 Channel 5 ON; SNMP verifies status.  
   4.10 Channel 5 OFF; SNMP verifies status.  
   4.11 Channel 6 ON; SNMP verifies status.  
   4.12 Channel 6 OFF; SNMP verifies status.  
   4.13 Channel 7 ON; SNMP verifies status.  
   4.14 Channel 7 OFF; SNMP verifies status.  
   4.15 Channel 8 ON; SNMP verifies status.  
   4.16 Channel 8 OFF; SNMP verifies status.  
5. All outlets  
   5.1 ALL channels ON; SNMP verifies status.  
   5.2 ALL channels OFF; SNMP verifies status.  
6. Device resets to factory settings; SYSINFO confirms reset.  
7. EEPROM dump  
   7.1 EEPROM content is saved and translated to ASCII.  
   7.2 Data matches expected values per EEPROM Memory Map.  
