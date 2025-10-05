#!/usr/bin/env python3
"""
ENERGIS PCAP Capture Integration Test (UTFW)
============================================

This test uses only framework-provided actions to:
  1) Start an ICMP stimulus in the background (Ping).
  2) Capture live traffic to a PCAP file (CapturePcap).
  3) Validate that ICMP was captured (analyze_PCAP).

No custom helpers; no STE wrappers for single actions.
"""

import sys
from pathlib import Path

from UTFW.core import run_test_with_teardown, get_hwconfig
from UTFW.modules.network.pcap_capture import CapturePcap, Ping
from UTFW.modules.network import pcap_analyze as analyzer
from UTFW.core import get_reports_dir
from UTFW.modules import snmp as SNMP
from UTFW.modules import serial as UART


class tc_pcap_capture:
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

        # Paths from hardware config
        Path(reports_dir).mkdir(parents=True, exist_ok=True)
        output_pcap = str(Path(reports_dir) / hw.PCAP_CAPTURE_FILENAME)

        # Use loopback by default; override in hw if desired
        iface = "lo"
        ping_target = "127.0.0.1"

        return [
            # Step 1: start ping stimulus in the background so it overlaps with capture
            Ping(
                name="Ping 127.0.0.1",
                target=ping_target,
                background=True,
                count=0,          # continuous
                duration_s=6.0,   # ≥ capture duration
                timeout_s=1.0,
                interval_s=0.2
                ),

            # Step 2: capture for a window inside the ping duration
            CapturePcap(
                name="Capture PCAP",
                output_path=output_pcap,
                interface=iface,
                bpf=f"icmp and host {ping_target}",
                duration_s=5,
                packet_count=None,
                snaplen=256,
                promiscuous=False,
                file_format="pcap",
                require_tool=None,
            ),

            # Step 3: verify ICMP was captured
            analyzer.analyze_PCAP(
                name="Check_ICMP_Captured",
                pcap_path=output_pcap,
                display_filter=f"icmp && ip.addr=={ping_target}",
                expect_count_min=1,
                frame_size=None
            ),
        ]


def main():
    test_instance = tc_pcap_capture()
    return run_test_with_teardown(
        test_class_instance=test_instance,
        test_name="tc_pcap_capture",
        reports_dir="report_tc_pcap_capture",
    )


if __name__ == "__main__":
    sys.exit(main())
