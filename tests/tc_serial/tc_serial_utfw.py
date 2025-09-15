#!/usr/bin/env python3
"""
tc_serial.py using UTFW Framework - Perfect Clean Pattern
=========================================================
Exactly what you wanted: Python config, local variables, no extra methods
"""

import sys
from pathlib import Path

# Import UTFW framework
from UTFW.core import run_test_with_teardown
from UTFW.core import get_hwconfig
from UTFW.core import STE
from UTFW.modules import snmp as SNMP
from UTFW.modules import serial as UART
from UTFW.modules import network


class tc_serial_test:
    """Clean test class - init, setup with return, main"""
    
    def __init__(self):
        pass

    def setup(self):
        hw = get_hwconfig()
        """Setup actions and return test list"""
        # Network test parameters
        test_ip = "192.168.0.72"
        test_subnet = "255.255.0.0"
        test_gateway = "10.10.10.1"
        test_dns = "1.1.1.1"

        # Outlet control: set via SERIAL, verify via SERIAL and SNMP
        outlet_actions = []
        for channel in range(1, 9):
            # --- Turn ON ---
            outlet_actions.extend([
                UART.send_command_uart(
                    name=f"Set Channel {channel} to ON",
                    port=hw.SERIAL_PORT,
                    command=f"SET_CH {channel} ON",
                    baudrate=hw.BAUDRATE
                ),
                UART.send_command_uart(
                    name=f"Verify Channel {channel} is ON by Serial",
                    port=hw.SERIAL_PORT,
                    command=f"GET_CH {channel}",
                    baudrate=hw.BAUDRATE
                ),
                UART.validate_tokens(
                    name=f"Validate Serial reports CH{channel} ON",
                    response="",                 # uses last cached GET_CH response
                    tokens=["ON"]                # keep simple & tolerant
                ),
                SNMP.get_outlet(
                    name=f"Verify CH{channel} ON via SNMP",
                    ip=hw.BASELINE_IP,
                    channel=channel,
                    expected_state=True,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    community=hw.SNMP_COMMUNITY
                ),
                # --- Turn OFF ---
                UART.send_command_uart(
                    name=f"Set Channel {channel} to OFF",
                    port=hw.SERIAL_PORT,
                    command=f"SET_CH {channel} OFF",
                    baudrate=hw.BAUDRATE
                ),
                UART.send_command_uart(
                    name=f"Verify Channel {channel} is OFF by Serial",
                    port=hw.SERIAL_PORT,
                    command=f"GET_CH {channel}",
                    baudrate=hw.BAUDRATE
                ),
                UART.validate_tokens(
                    name=f"Validate Serial reports CH{channel} OFF",
                    response="",                 
                    tokens=["OFF"]
                ),
                SNMP.get_outlet(
                    name=f"Verify CH{channel} OFF via SNMP",
                    ip=hw.BASELINE_IP,
                    channel=channel,
                    expected_state=False,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    community=hw.SNMP_COMMUNITY
                )
            ])

        test_all_outlets = STE(    
                # --- ALL On ---
                UART.send_command_uart(
                    name=f"Set All Channels to ON",
                    port=hw.SERIAL_PORT,
                    command=f"SET_CH ALL ON",
                    baudrate=hw.BAUDRATE
                ),
                UART.get_all_channels(
                    name=f"Read All Channels ON state by Serial",
                    port=hw.SERIAL_PORT,
                    baudrate=hw.BAUDRATE,
                    expected= [True, True, True, True, True, True, True, True]
                ),
                SNMP.verify_all_outlets(
                    name="Verify ALL ON via SNMP (1..8 == 1)",
                    ip=hw.BASELINE_IP,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    expected_state=True,
                    community=hw.SNMP_COMMUNITY
                ),
                # --- ALL Off ---
                UART.send_command_uart(
                    name=f"Set All Channels to OFF",
                    port=hw.SERIAL_PORT,
                    command=f"SET_CH ALL OFF",
                    baudrate=hw.BAUDRATE
                ),
                UART.get_all_channels(
                    name=f"Read All Channels ON state by Serial",
                    port=hw.SERIAL_PORT,
                    baudrate=hw.BAUDRATE,
                    expected= [False, False, False, False, False, False, False, False]
                ),
                SNMP.verify_all_outlets(
                    name="Verify ALL OFF via SNMP (1..8 == 0)",
                    ip=hw.BASELINE_IP,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    expected_state=False,
                    community=hw.SNMP_COMMUNITY
                ),
                name="Test ALL Outlets via Serial and verify with SNMP"
        )
        
        # Return test action list
        return [
            # Step 1: HELP with sub-steps for each token
            STE(
                UART.send_command_uart(
                    name="Send HELP command",
                    port=hw.SERIAL_PORT,
                    command=hw.HELP_CMD,
                    baudrate=hw.BAUDRATE
                ),
                UART.validate_tokens(
                    name="Validate HELP tokens present",
                    response="",     # if you cache the HELP response
                    tokens=hw.HELP_TOKENS
                )
            ),

            # Step 2: Validate SYSINFO
            UART.test_sysinfo_complete(
                name="Send SYSINFO and validate all parameters",
                port=hw.SERIAL_PORT,
                validation={
                    'firmware_regex': hw.FIRMWARE_REGEX,
                    'core_voltage_range': hw.CORE_VOLTAGE_RANGE,
                    'frequencies': {
                        'sys_hz_min': hw.SYS_HZ_MIN,
                        'usb_hz_expect': hw.USB_HZ_EXPECT,
                        'per_hz_expect': hw.PER_HZ_EXPECT,
                        'adc_hz_expect': hw.ADC_HZ_EXPECT
                    }
                },
                baudrate=hw.BAUDRATE
            ),

            # Step 3: Network configuration - sub-steps
            STE(UART.set_network_parameter(
                    name=f"Change Network parameter to {test_ip}, {test_subnet}, {test_gateway}, {test_dns}",
                    port=hw.SERIAL_PORT,
                    param="CONFIG_NETWORK",
                    value=f"{test_ip}${test_subnet}${test_gateway}${test_dns}",
                    baudrate=hw.BAUDRATE,
                    reboot_timeout=20
                ),
                UART.verify_network_change(
                    name=f"Verify Network parameter change to {test_ip}",
                    port=hw.SERIAL_PORT,
                    param="IP",
                    expected_value=test_ip,
                    baudrate=hw.BAUDRATE
                ),
                UART.verify_network_change(
                    name=f"Verify Network parameter change to {test_gateway}",
                    port=hw.SERIAL_PORT,
                    param="Gateway",
                    expected_value=test_gateway,
                    baudrate=hw.BAUDRATE
                ),
                UART.verify_network_change(
                    name=f"Verify Network parameter change to {test_subnet}",
                    port=hw.SERIAL_PORT,
                    param="Subnet Mask",
                    expected_value=test_subnet,
                    baudrate=hw.BAUDRATE
                ),
                UART.verify_network_change(
                    name=f"Verify Network parameter change to {test_dns}",
                    port=hw.SERIAL_PORT,
                    param="DNS",
                    expected_value=test_dns,
                    baudrate=hw.BAUDRATE
                ),
                network.ping_host(
                    name=f"Ping new IP {test_ip}",
                    ip=test_ip
                ),
                UART.set_network_parameter(
                    name=f"Revert Network parameters to {hw.BASELINE_IP}, {hw.BASELINE_SUBNET}, {hw.BASELINE_GATEWAY}, {hw.BASELINE_DNS}",
                    port=hw.SERIAL_PORT,
                    param="CONFIG_NETWORK",
                    value=f"{hw.BASELINE_IP}${hw.BASELINE_SUBNET}${hw.BASELINE_GATEWAY}${hw.BASELINE_DNS}",
                    baudrate=hw.BAUDRATE,
                    reboot_timeout=20
                ),
                name="Test Network configuration change via Serial",
            ),
            
            # Step 4: Outlet control - sub-steps
            STE( *outlet_actions,
                name="Test outlet control via Serial and verify with SNMP"
            ),

            # Step 5: Test ALL outlets together
            test_all_outlets,

            # Step 6: Factory reset - single action
            UART.factory_reset_complete(
                name="Reset to factory settings (RFS) and verify data",
                port=hw.SERIAL_PORT,
                baudrate=hw.BAUDRATE
            ),

            # Step 7: Parse EEPROM dump
            UART.analyze_eeprom_dump(
                name="Create EEPROM dump and analyze EEPROM content",
                port=hw.SERIAL_PORT,
                baudrate=hw.BAUDRATE,
                checks="eeprom_checks.json",
                reports_dir=Path("report_tc_serial_utfw")
            )
        ]


def main():
    """Create test instance and run"""
    test_instance = tc_serial_test()
    return run_test_with_teardown(
        test_class_instance=test_instance, 
        test_name="tc_serial_utfw",
        reports_dir="report_tc_serial_utfw"
    )


if __name__ == "__main__":
    sys.exit(main())

