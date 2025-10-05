#!/usr/bin/env python3
"""
tc_stress_switching.py - Rapid Channel Switching Stress Test
=============================================================
Tests relay durability and system stability under rapid SNMP-based switching
Uses SNMP for maximum speed (no UART delays)
"""

import sys
from pathlib import Path

from UTFW.core import run_test_with_teardown
from UTFW.core import get_hwconfig
from UTFW.core import STE
from UTFW.modules import snmp as SNMP
from UTFW.modules import serial as UART


class tc_stress_switching_test:
    """Stress test for rapid outlet switching using SNMP"""

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

        # Rapid switching: 50 cycles per channel (much faster with SNMP)
        rapid_switching_actions = []
        num_cycles = 15

        for cycle in range(1, num_cycles + 1):
            for channel in range(1, 9):
                rapid_switching_actions.extend([
                    SNMP.set_outlet(
                        name=f"Cycle {cycle} - CH{channel} ON",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        state=True,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    ),
                    SNMP.get_outlet(
                        name=f"Cycle {cycle} - Verify CH{channel} ON",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        expected_state=True,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    ),
                    SNMP.set_outlet(
                        name=f"Cycle {cycle} - CH{channel} OFF",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        state=False,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    ),
                    SNMP.get_outlet(
                        name=f"Cycle {cycle} - Verify CH{channel} OFF",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        expected_state=False,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    )
                ])

        # All channels simultaneous switching
        all_switching_actions = []
        for cycle in range(1, num_cycles + 1):
            # Turn all ON
            for channel in range(1, 9):
                all_switching_actions.append(
                    SNMP.set_outlet(
                        name=f"Cycle {cycle} - Set CH{channel} ON",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        state=True,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    )
                )
            # Verify all ON
            all_switching_actions.append(
                SNMP.verify_all_outlets(
                    name=f"Cycle {cycle} - Verify all ON",
                    ip=hw.BASELINE_IP,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    expected_state=True,
                    community=hw.SNMP_COMMUNITY
                )
            )
            # Turn all OFF
            for channel in range(1, 9):
                all_switching_actions.append(
                    SNMP.set_outlet(
                        name=f"Cycle {cycle} - Set CH{channel} OFF",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        state=False,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    )
                )
            # Verify all OFF
            all_switching_actions.append(
                SNMP.verify_all_outlets(
                    name=f"Cycle {cycle} - Verify all OFF",
                    ip=hw.BASELINE_IP,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    expected_state=False,
                    community=hw.SNMP_COMMUNITY
                )
            )

        # Alternating pattern stress test
        alternating_actions = []
        for cycle in range(1, num_cycles + 1):
            # Odd channels ON, even OFF
            for channel in range(1, 9):
                state = True if channel % 2 == 1 else False
                alternating_actions.append(
                    SNMP.set_outlet(
                        name=f"Cycle {cycle} - Set CH{channel} {'ON' if state else 'OFF'}",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        state=state,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    )
                )
            # Verify pattern
            for channel in range(1, 9):
                expected = True if channel % 2 == 1 else False
                alternating_actions.append(
                    SNMP.get_outlet(
                        name=f"Cycle {cycle} - Verify CH{channel} {'ON' if expected else 'OFF'}",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        expected_state=expected,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    )
                )
            # Reverse: Even ON, odd OFF
            for channel in range(1, 9):
                state = False if channel % 2 == 1 else True
                alternating_actions.append(
                    SNMP.set_outlet(
                        name=f"Cycle {cycle} - Set CH{channel} {'ON' if state else 'OFF'}",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        state=state,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    )
                )
            # Verify reversed pattern
            for channel in range(1, 9):
                expected = False if channel % 2 == 1 else True
                alternating_actions.append(
                    SNMP.get_outlet(
                        name=f"Cycle {cycle} - Verify CH{channel} {'ON' if expected else 'OFF'}",
                        ip=hw.BASELINE_IP,
                        channel=channel,
                        expected_state=expected,
                        outlet_base_oid=hw.OUTLET_BASE_OID,
                        community=hw.SNMP_COMMUNITY
                    )
                )

        return [
            # Step 1: Rapid individual channel switching
            STE(
                *rapid_switching_actions,
                name=f"Rapid SNMP switching test - {num_cycles} cycles per channel"
            ),

            # Step 2: All channels simultaneous switching
            STE(
                *all_switching_actions,
                name=f"Simultaneous all-channel SNMP switching - {num_cycles} cycles"
            ),

            # Step 3: Alternating pattern stress
            STE(
                *alternating_actions,
                name=f"Alternating pattern SNMP stress test - {num_cycles} cycles"
            ),

            # Step 4: Final state verification
            STE(
                *[SNMP.set_outlet(
                    name=f"Set CH{channel} OFF after stress test",
                    ip=hw.BASELINE_IP,
                    channel=channel,
                    state=False,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    community=hw.SNMP_COMMUNITY
                ) for channel in range(1, 9)],
                SNMP.verify_all_outlets(
                    name="Final SNMP verification - all OFF",
                    ip=hw.BASELINE_IP,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    expected_state=False,
                    community=hw.SNMP_COMMUNITY
                ),
                name="Final state verification after stress test"
            )
        ]


def main():
    """Create test instance and run"""
    test_instance = tc_stress_switching_test()
    return run_test_with_teardown(
        test_class_instance=test_instance,
        test_name="tc_stress_switching",
        reports_dir="report_tc_stress_switching"
    )


if __name__ == "__main__":
    sys.exit(main())
