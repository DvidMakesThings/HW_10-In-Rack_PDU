#!/usr/bin/env python3
"""
tc_eeprom_persistence.py - EEPROM Persistence and Factory Reset Test
=====================================================================
Tests configuration persistence across reboots and factory reset behavior
"""

import sys
from pathlib import Path

from UTFW.core import run_test_with_teardown
from UTFW.core import get_hwconfig
from UTFW.core import get_reports_dir
from UTFW.core import STE
from UTFW.modules import serial as UART
from UTFW.modules import snmp as SNMP


class tc_eeprom_persistence_test:
    """EEPROM persistence and factory reset test"""

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
        reports_dir = get_reports_dir()

        # Resolve checks file path relative to this script
        test_script_dir = Path(__file__).parent
        checks_file = str(test_script_dir.parent / "eeprom_checks.json")

        # Test configuration values
        test_ip = "192.168.0.12"
        test_subnet = "255.255.255.0"
        test_gateway = "192.168.1.1"
        test_dns = "1.1.1.1"

        # Channel states to test
        test_channel_states = []
        for channel in range(1, 9):
            state = "ON" if channel % 2 == 1 else "OFF"
            test_channel_states.append(
                UART.send_command_uart(
                    name=f"Set CH{channel} to {state}",
                    port=hw.SERIAL_PORT,
                    command=f"SET_CH {channel} {state}",
                    baudrate=hw.BAUDRATE
                )
            )

        # Verify channel states after setting
        verify_channel_states = []
        for channel in range(1, 9):
            expected_state = True if channel % 2 == 1 else False
            verify_channel_states.append(
                SNMP.get_outlet(
                    name=f"Verify CH{channel} state via SNMP",
                    ip=test_ip,
                    channel=channel,
                    expected_state=expected_state,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    community=hw.SNMP_COMMUNITY
                )
            )
        
        verify_channel_states_negative = []
        for channel in range(1, 9):
            verify_channel_states_negative.append(
                SNMP.get_outlet(
                    name=f"MUST FAIL - Verify CH{channel} state via SNMP",
                    ip=test_ip,
                    channel=channel,
                    expected_state=True,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    community=hw.SNMP_COMMUNITY,
                    negative_test=True
                )
            )

        return [
            # Step 1: Set custom network configuration
            STE(
                UART.set_network_parameter(
                    name=f"Set custom network config",
                    port=hw.SERIAL_PORT,
                    param="CONFIG_NETWORK",
                    value=f"{test_ip}${test_subnet}${test_gateway}${test_dns}",
                    baudrate=hw.BAUDRATE,
                    reboot_timeout=20
                ),
                UART.verify_network_change(
                    name="Verify custom IP",
                    port=hw.SERIAL_PORT,
                    param="IP",
                    expected_value=test_ip,
                    baudrate=hw.BAUDRATE
                ),
                UART.verify_network_change(
                    name="Verify custom Gateway",
                    port=hw.SERIAL_PORT,
                    param="Gateway",
                    expected_value=test_gateway,
                    baudrate=hw.BAUDRATE
                ),
                name="Set custom network configuration"
            ),

            # Step 2: Set alternating channel states
            STE(
                *test_channel_states,
                *verify_channel_states,
                name="Set alternating channel states (odd ON, even OFF)"
            ),

            # Step 3: Reboot and verify persistence
            STE(
                UART.send_command_uart(
                    name="Reboot device",
                    port=hw.SERIAL_PORT,
                    command=hw.REBOOT_CMD,
                    baudrate=hw.BAUDRATE,
                    reboot=True
                ),
                UART.send_command_uart(
                    name="Get network config after reboot",
                    port=hw.SERIAL_PORT,
                    command=hw.NETINFO_CMD,
                    baudrate=hw.BAUDRATE
                ),
                UART.validate_tokens(
                    name="Verify network config persisted",
                    response="",
                    tokens=[test_ip, test_gateway]
                ),
                name="Reboot and verify network config persistence"
            ),

            # Step 4: Verify channel states persisted
            STE(
                *verify_channel_states_negative,
                name="Verify channel states not persisted after reboot"
            ),

            # Step 5: Factory reset
            STE(
                UART.factory_reset_complete(
                    name="Perform factory reset (RFS)",
                    port=hw.SERIAL_PORT,
                    baudrate=hw.BAUDRATE
                ),
                name="Factory reset device"
            ),

            # Step 6: Verify factory defaults restored
            STE(
                UART.send_command_uart(
                    name="Get network config after factory reset",
                    port=hw.SERIAL_PORT,
                    command=hw.NETINFO_CMD,
                    baudrate=hw.BAUDRATE
                ),
                UART.validate_tokens(
                    name="Verify baseline network config restored",
                    response="",
                    tokens=[hw.BASELINE_IP, hw.BASELINE_GATEWAY]
                ),
                UART.get_all_channels(
                    name="Verify all channels reset to OFF",
                    port=hw.SERIAL_PORT,
                    baudrate=hw.BAUDRATE,
                    expected=[False] * 8
                ),
                name="Verify factory defaults restored"
            ),

            # Step 7: Multiple reboot cycles to test stability
            STE(
                UART.send_command_uart(
                    name="Reboot cycle 1",
                    port=hw.SERIAL_PORT,
                    command=hw.REBOOT_CMD,
                    baudrate=hw.BAUDRATE,
                    reboot=True
                ),
                UART.send_command_uart(
                    name="Verify ready after reboot 1",
                    port=hw.SERIAL_PORT,
                    command=hw.NETINFO_CMD,
                    baudrate=hw.BAUDRATE
                ),
                UART.send_command_uart(
                    name="Reboot cycle 2",
                    port=hw.SERIAL_PORT,
                    command=hw.REBOOT_CMD,
                    baudrate=hw.BAUDRATE,
                    reboot=True
                ),
                UART.send_command_uart(
                    name="Verify ready after reboot 2",
                    port=hw.SERIAL_PORT,
                    command=hw.NETINFO_CMD,
                    baudrate=hw.BAUDRATE
                ),
                UART.send_command_uart(
                    name="Reboot cycle 3",
                    port=hw.SERIAL_PORT,
                    command=hw.REBOOT_CMD,
                    baudrate=hw.BAUDRATE,
                    reboot=True
                ),
                UART.send_command_uart(
                    name="Verify ready after reboot 3",
                    port=hw.SERIAL_PORT,
                    command=hw.NETINFO_CMD,
                    baudrate=hw.BAUDRATE
                ),
                UART.validate_tokens(
                    name="Verify config stable after multiple reboots",
                    response="",
                    tokens=[hw.BASELINE_IP]
                ),
                name="Multiple reboot cycle stability test"
            ),

            # Step 8: EEPROM dump analysis
            STE(
                UART.analyze_eeprom_dump(
                    name="Analyze EEPROM dump after all tests",
                    port=hw.SERIAL_PORT,
                    baudrate=hw.BAUDRATE,
                    checks=checks_file,
                    reports_dir=Path(reports_dir)
                ),
                name="Final EEPROM dump and analysis"
            )
        ]


def main():
    """Create test instance and run"""
    test_instance = tc_eeprom_persistence_test()
    return run_test_with_teardown(
        test_class_instance=test_instance,
        test_name="tc_eeprom_persistence",
        reports_dir="report_tc_eeprom_persistence"
    )


if __name__ == "__main__":
    sys.exit(main())
