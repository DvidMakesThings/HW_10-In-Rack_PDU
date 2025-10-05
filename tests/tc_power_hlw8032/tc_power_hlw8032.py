#!/usr/bin/env python3
"""
tc_power_hlw8032.py - Power Monitoring Test using HLW8032
==========================================================
Tests power monitoring functionality via READ_HLW8032 command
"""

import sys
from pathlib import Path

from UTFW.core import run_test_with_teardown
from UTFW.core import get_hwconfig
from UTFW.core import STE
from UTFW.modules import serial as UART
from UTFW.modules import snmp as SNMP


class tc_power_hlw8032_test:
    """Power monitoring test for HLW8032 chip"""

    def __init__(self):
        pass

    def pre(self):
        """Pre-steps: Reboot device and wait for it to be ready.

        These steps prepare the hardware before the actual test begins.
        """
        hw = get_hwconfig()

        return [
            # PRE-STEP 1: Send reboot command via UART
            UART.send_command_uart(
                name="Reboot device via UART",
                port=hw.SERIAL_PORT,
                command="REBOOT",
                baudrate=hw.BAUDRATE,
                reboot=True  # Special handling for reboot
            ),
        ]

    def teardown(self):
        """Teardown: Cleanup actions that always run, even on failure.

        These steps are guaranteed to execute regardless of test outcome.
        Critical for ensuring hardware is left in a safe state.
        """
        hw = get_hwconfig()

        return [
            # TEARDOWN 1.1: Ensure all outputs are OFF (safety measure)
            SNMP.set_all_outlets(
                name="Ensure all outputs OFF",
                ip=hw.BASELINE_IP,
                all_on_oid=hw.ALL_ON_OID,
                all_off_oid=hw.ALL_OFF_OID,
                state=False,
                community=hw.SNMP_COMMUNITY
            ),

            # TEARDOWN 1.2: Final verification all outputs are OFF
            SNMP.verify_all_outlets(
                name="Final verify all outputs OFF",
                ip=hw.BASELINE_IP,
                outlet_base_oid=hw.OUTLET_BASE_OID,
                expected_state=False,
                community=hw.SNMP_COMMUNITY
            ),
        ]

    def setup(self):
        hw = get_hwconfig()

        # Power monitoring test actions
        power_actions = []

        # Test each channel's power monitoring
        for channel in range(1, 9):
            power_actions.extend([
                # Turn channel ON first
                UART.send_command_uart(
                    name=f"Turn ON Channel {channel}",
                    port=hw.SERIAL_PORT,
                    command=f"SET_CH {channel} ON",
                    baudrate=hw.BAUDRATE
                ),
                # Read power data
                UART.send_command_uart(
                    name=f"Read power data for Channel {channel}",
                    port=hw.SERIAL_PORT,
                    command=f"READ_HLW8032 {channel}",
                    baudrate=hw.BAUDRATE
                ),
                UART.validate_tokens(
                    name=f"Validate power response for CH{channel}",
                    response="",
                    tokens=["V=", "I=", "P="]
                ),
                # Turn channel OFF
                UART.send_command_uart(
                    name=f"Turn OFF Channel {channel}",
                    port=hw.SERIAL_PORT,
                    command=f"SET_CH {channel} OFF",
                    baudrate=hw.BAUDRATE
                )
            ])

        # Test reading all channels
        read_all_actions = []
        for channel in range(1, 9):
            read_all_actions.append(
                UART.send_command_uart(
                    name=f"Read power data for Channel {channel}",
                    port=hw.SERIAL_PORT,
                    command=f"READ_HLW8032 {channel}",
                    baudrate=hw.BAUDRATE
                )
            )

        return [
            # Step 1: Test individual channel power monitoring
            STE(
                *power_actions,
                name="Test power monitoring per channel with ON/OFF cycles"
            ),

            # Step 2: Read all channels continuously
            STE(
                *read_all_actions,
                name="Read power data from all channels"
            ),

            # Step 3: Test bulk READ_HLW8032 command (no channel specified)
            STE(
                UART.send_command_uart(
                    name="Read all power data at once",
                    port=hw.SERIAL_PORT,
                    command="READ_HLW8032",
                    baudrate=hw.BAUDRATE
                ),
                UART.validate_tokens(
                    name="Validate bulk power response",
                    response="",
                    tokens=["V=", "I=", "P="]
                ),
                name="Test bulk power monitoring command"
            )
        ]


def main():
    """Create test instance and run"""
    test_instance = tc_power_hlw8032_test()
    return run_test_with_teardown(
        test_class_instance=test_instance,
        test_name="tc_power_hlw8032",
        reports_dir="report_tc_power_hlw8032"
    )


if __name__ == "__main__":
    sys.exit(main())
