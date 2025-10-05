#!/usr/bin/env python3
"""
tc_sanity_check.py
==================
System Voltage Sanity Check Test

This test validates all ADC voltage and temperature readings from the device
to ensure the system is operating within acceptable ranges.

Expected tolerances:
- Voltage readings: ±5%
- Temperature reading: ±20%
"""


import sys

from UTFW.core import run_test_with_teardown
from UTFW.core import get_hwconfig
from UTFW.modules import snmp as SNMP
from UTFW.modules.serial import serial as UART


class tc_sanity_check:
    """System Voltage Sanity Check Test"""

    def __init__(self):
        self.test_id = "tc_sanity_check"
        self.description = "Validate all system voltage and temperature readings"

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
        """Main test steps: Validate all voltage and temperature readings."""
        hw = get_hwconfig()

        return [
            SNMP.read_oid(
                name="Die Sensor Voltage",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_DIE_SENSOR_VOLTAGE,
                min_val=0.68 * 0.80,
                max_val=0.68 * 1.20,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="Die Sensor Temperature",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_DIE_SENSOR_TEMPERATURE,
                min_val=40.0 * 0.80,
                max_val=40.0 * 1.20,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="12V PSU Voltage",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_12V_PSU_VOLTAGE,
                min_val=12.0 * 0.95,
                max_val=12.0 * 1.05,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="5V USB Voltage",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_5V_USB_VOLTAGE,
                min_val=5.0 * 0.95,
                max_val=5.0 * 1.05,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="12V PSU Divider Voltage",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_12V_PSU_DIVIDER_VOLTAGE,
                min_val=1.09 * 0.95,
                max_val=1.09 * 1.05,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="5V USB Divider Voltage",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_5V_USB_DIVIDER_VOLTAGE,
                min_val=2.48 * 0.95,
                max_val=2.48 * 1.05,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="Core VREG Target Voltage",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_CORE_VREG_TARGET_VOLTAGE,
                min_val=0.90 * 0.95,
                max_val=0.90 * 1.05,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="Bandgap Reference",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_BANDGAP_REFERENCE,
                min_val=1.10 * 0.95,
                max_val=1.10 * 1.05,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="USB PHY Rail",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_USB_PHY_RAIL,
                min_val=1.80 * 0.95,
                max_val=1.80 * 1.05,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="IO Rail Nominal",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_IO_RAIL_NOMINAL,
                min_val=3.30 * 0.95,
                max_val=3.30 * 1.05,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.read_oid(
                name="Core VREG Status",
                ip=hw.BASELINE_IP,
                oid=hw.ADC_CORE_VREG_STATUS_FLAGS,
                expected="OK",
                community=hw.SNMP_COMMUNITY
            ),
        ]

def main():
    """Create test instance and run"""
    test_instance = tc_sanity_check()
    return run_test_with_teardown(
        test_class_instance=test_instance,
        test_name="tc_sanity_check",
        reports_dir="report_tc_sanity_check",
    )


if __name__ == "__main__":
    sys.exit(main())
