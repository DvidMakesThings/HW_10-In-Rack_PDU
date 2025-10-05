# TestCases/tc_network_eth.py
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
Network PCAP Functional Test Suite (UTFW)
--------------------------------------------------------------------------------
This test suite uses UTFW modules to capture and analyze network traffic (PCAP)
from a DUT. It performs the following:

- Reboots the DUT via UART.
- Captures network traffic while accessing the DUT's settings and control pages.
- Analyzes the captured PCAP files to verify the presence of expected network
    configuration values (IP address, gateway, subnet mask, DNS, device name, etc.)
    in HTTP responses.

All hardware-specific parameters are loaded from the provided hardware config.
================================================================================
"""

import sys
from pathlib import Path

from UTFW.core import run_test_with_teardown
from UTFW.core import get_hwconfig
from UTFW.core import get_reports_dir
from UTFW.core import STE, PTE, startFirstWith
from UTFW.modules.snmp import snmp as SNMP
from UTFW.modules.ethernet import ethernet as ETH
from UTFW.modules.serial import serial as UART
from UTFW.modules.network import pcap_capture as PCAP
from UTFW.modules.network import pcap_analyze as ANALYZER
from UTFW.modules.nop import nop as NOP



class tc_network_pcap:
    """UTFW test suite for Network tests using PCAP files."""

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

        return [
            # Step 1:
            NOP.NOP(
                name="Wait for device to boot fully",
                duration_ms= 2000,
                ),
            # Step 2: Capture Settings page
            PTE(
                # 2.1: Start PCAP capture
                startFirstWith(
                    PCAP.CapturePcap(
                        name="Capture PCAP",
                        output_path=str(
                            Path(reports_dir) / "capture_settings.pcap"
                        ),
                        interface="Ethernet",
                        bpf=f"host {hw.BASELINE_IP}",
                        duration_s=8,
                        packet_count=None,
                        snaplen=1518,
                        promiscuous=False,
                        file_format="pcap",
                        require_tool=None,
                    )
                ),
                ETH.http_get_action(
                    name="Access the settings page",
                    base_url=f"http://{hw.BASELINE_IP}:{hw.HTTP_PORT}",
                    path="/settings.html",
                    timeout=5.0,
                ),
                name="Start capturing settings page",
                stagger_s=1,  # ensures capture starts, waits 1s, then HTTP gets start
            ),
            
            # Step 3: Analyze Settings page
            STE(
                # 3.1: Validate IP address
                ANALYZER.pcap_checkFrames(
                    name="Validate IP address",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "192.168.0.11"}]},
                    ],
                ),
                # 3.2: Validate Gateway address
                ANALYZER.pcap_checkFrames(
                    name="Validate Gateway address",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "192.168.0.1"}]},
                    ],
                ),
                # 3.3: Validate Subnet mask
                ANALYZER.pcap_checkFrames(
                    name="Validate Subnet mask",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "255.255.255.0"}]},
                    ],
                ),
                # 3.4: Validate DNS address
                ANALYZER.pcap_checkFrames(
                    name="Validate DNS address",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "8.8.8.8"}]},
                    ],
                ),
                # 3.5: Validate Device Name
                ANALYZER.pcap_checkFrames(
                    name="Settings: Device Name present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "ENERGIS-1.0.0"}]},
                    ],
                ),
                # 3.6: Validate Location
                ANALYZER.pcap_checkFrames(
                    name="Settings: Device Location present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Location"}]},
                    ],
                ),
                # 3.7: Validate page title
                ANALYZER.pcap_checkFrames(
                    name="Settings: Page title present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Settings"}]},
                    ],
                ),
                # 3.8: Validate Network Settings section
                ANALYZER.pcap_checkFrames(
                    name="Settings: Network Settings section present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Network Settings"}]},
                    ],
                ),
                # 3.9: Validate Device Settings section
                ANALYZER.pcap_checkFrames(
                    name="Settings: Device Settings section present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Device Settings"}]},
                    ],
                ),
                # 3.10: Validate Temperature Unit section
                ANALYZER.pcap_checkFrames(
                    name="Settings: Temperature Unit section present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Temperature Unit"}]},
                    ],
                ),
                # 3.11: Validate Celsius option
                ANALYZER.pcap_checkFrames(
                    name="Settings: Celsius option present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Celsius"}]},
                    ],
                ),
                # 3.12: Validate Fahrenheit option
                ANALYZER.pcap_checkFrames(
                    name="Settings: Fahrenheit option present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Fahrenheit"}]},
                    ],
                ),
                # 3.13: Validate Kelvin option
                ANALYZER.pcap_checkFrames(
                    name="Settings: Kelvin option present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Kelvin"}]},
                    ],
                ),
                # 3.14: Validate Save Settings button
                ANALYZER.pcap_checkFrames(
                    name="Settings: Save Settings button present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Save Settings"}]},
                    ],
                ),
                # 3.15: Validate Manuals section
                ANALYZER.pcap_checkFrames(
                    name="Settings: Manuals section present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Manuals"}]},
                    ],
                ),
                # 3.16: Validate User Manual link
                ANALYZER.pcap_checkFrames(
                    name="Settings: User Manual link present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "User Manual"}]},
                    ],
                ),
                # 3.17: Validate Programming Manual link
                ANALYZER.pcap_checkFrames(
                    name="Settings: Programming Manual link present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Programming"}]},
                    ],
                ),
                # 3.18: Validate form field labels
                ANALYZER.pcap_checkFrames(
                    name="Settings: All form labels present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [
                            {"contains_ascii": "IP Address"},
                            {"contains_ascii": "Default Gateway"},
                            {"contains_ascii": "Subnet Mask"},
                            {"contains_ascii": "DNS Server"},
                            {"contains_ascii": "Device Name"},
                            {"contains_ascii": "Device Location"},
                        ]},
                    ],
                ),
                name="Analyze captured settings page",
            ),

            # Step 4: Capture Control page
            PTE(
                # 4.1: Start PCAP capture for control page
                startFirstWith(
                    PCAP.CapturePcap(
                        name="Capture Control Page PCAP",
                        output_path=str(
                            Path(reports_dir) / "capture_control.pcap"
                        ),
                        interface="Ethernet",
                        bpf=f"host {hw.BASELINE_IP}",
                        duration_s=8,
                        packet_count=None,
                        snaplen=1518,
                        promiscuous=False,
                        file_format="pcap",
                        require_tool=None,
                    )
                ),
                ETH.http_get_action(
                    name="Access the control page",
                    base_url=f"http://{hw.BASELINE_IP}:{hw.HTTP_PORT}",
                    path="/control.html",
                    timeout=5.0,
                ),
                name="Start capturing control page",
                stagger_s=1,  # ensures capture starts, waits 1s, then HTTP gets start
            ),

            # Step 5: Analyze Control page
            STE(
                # 5.1: Validate page title
                ANALYZER.pcap_checkFrames(
                    name="Control: Page title present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Control"}]},
                    ],
                ),
                # 5.2: Validate page description
                ANALYZER.pcap_checkFrames(
                    name="Control: Page description present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Manage power channels"}]},
                    ],
                ),
                # 5.3: Validate table headers
                ANALYZER.pcap_checkFrames(
                    name="Control: Channel header present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Channel"}]},
                    ],
                ),
                # 5.4: Validate Switch header
                ANALYZER.pcap_checkFrames(
                    name="Control: Switch header present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Switch"}]},
                    ],
                ),
                # 5.5: Validate Voltage header
                ANALYZER.pcap_checkFrames(
                    name="Control: Voltage header present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Voltage"}]},
                    ],
                ),
                # 5.6: Validate Current header
                ANALYZER.pcap_checkFrames(
                    name="Control: Current header present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Current"}]},
                    ],
                ),
                # 5.7: Validate Uptime header
                ANALYZER.pcap_checkFrames(
                    name="Control: Uptime header present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Uptime"}]},
                    ],
                ),
                # 5.8: Validate Power header
                ANALYZER.pcap_checkFrames(
                    name="Control: Power header present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Power"}]},
                    ],
                ),
                # 5.9: Validate all 8 channels present
                ANALYZER.pcap_checkFrames(
                    name="Control: All 8 channels present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [
                            {"contains_ascii": "1"},
                            {"contains_ascii": "2"},
                            {"contains_ascii": "3"},
                            {"contains_ascii": "4"},
                            {"contains_ascii": "5"},
                            {"contains_ascii": "6"},
                            {"contains_ascii": "7"},
                            {"contains_ascii": "8"},
                        ]},
                    ],
                ),
                # 5.10: Validate Apply Changes button
                ANALYZER.pcap_checkFrames(
                    name="Control: Apply Changes button present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Apply Changes"}]},
                    ],
                ),
                # 5.11: Validate All On button
                ANALYZER.pcap_checkFrames(
                    name="Control: All On button present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "All On"}]},
                    ],
                ),
                # 5.12: Validate All Off button
                ANALYZER.pcap_checkFrames(
                    name="Control: All Off button present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "All Off"}]},
                    ],
                ),
                # 5.13: Validate Internal Temperature label
                ANALYZER.pcap_checkFrames(
                    name="Control: Internal Temperature label present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Internal Temperature"}]},
                    ],
                ),
                # 5.14: Validate System Status label
                ANALYZER.pcap_checkFrames(
                    name="Control: System Status label present",
                    pcap_path=str(
                        Path(reports_dir) / "capture_control.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "System Status"}]},
                    ],
                ),
                name="Analyze captured control page",
            ),
        ]


def main():
    test_instance = tc_network_pcap()
    return run_test_with_teardown(
        test_class_instance=test_instance,
        test_name="tc_network_pcap",
        reports_dir="report_tc_network_pcap",
    )


if __name__ == "__main__":
    sys.exit(main())
