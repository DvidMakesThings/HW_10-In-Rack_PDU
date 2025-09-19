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
from UTFW.core import STE, PTE, startFirstWith
from UTFW.modules.snmp import snmp as SNMP
from UTFW.modules.ethernet import ethernet as ETH
from UTFW.modules.serial import serial as UART
from UTFW.modules.network import pcap_capture as PCAP
from UTFW.modules.network import pcap_analyze as ANALYZER
from UTFW.modules.network import network as NET


class tc_network_pcap:
    """UTFW test suite for Network tests using PCAP files."""

    def __init__(self):
        pass

    def setup(self):

        hw = get_hwconfig()

        base = f"http://{hw.BASELINE_IP}:{hw.HTTP_PORT}"
        new_base = f"http://{hw.TEMP_NEW_IP}:{hw.HTTP_PORT}"

        # Hardware protection: one relay switch operation at most every 0.2 s
        RELAY_PACE_KEY = "relay"
        RELAY_MIN_INTERVAL = 0.200  # seconds

        HTTP_DUMPS = "http_dumps"

        return [
            # Step 1:
            UART.send_command_uart(
                name=f"Send reboot command via UART",
                port=hw.SERIAL_PORT,
                command="REBOOT",
                baudrate=hw.BAUDRATE,
                reboot=True,
            ),
            # Step 2: Capture Settings page
            PTE(
                startFirstWith(
                    PCAP.CapturePcap(
                        name="Capture PCAP",
                        output_path=str(
                            Path("report_tc_network_pcap") / "capture_settings.pcap"
                        ),
                        interface="Ethernet",
                        bpf=f"host {hw.BASELINE_IP}",
                        duration_s=5,
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
            # Step 2: Capture Control page
            PTE(
                startFirstWith(
                    PCAP.CapturePcap(
                        name="Capture PCAP",
                        output_path=str(
                            Path("report_tc_network_pcap") / "capture_control.pcap"
                        ),
                        interface="Ethernet",
                        bpf=f"host {hw.BASELINE_IP}",
                        duration_s=5,
                        packet_count=None,
                        snaplen=0,
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
            # Step 3: Analyze Settings page
            STE(
                ANALYZER.pcap_checkFrames(
                    name="Validate IP address",
                    pcap_path=str(
                        Path("report_tc_network_pcap") / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "192.168.0.11"}]},
                    ],
                ),
                ANALYZER.pcap_checkFrames(
                    name="Validate Gateway address",
                    pcap_path=str(
                        Path("report_tc_network_pcap") / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "192.168.0.1"}]},
                    ],
                ),
                ANALYZER.pcap_checkFrames(
                    name="Validate Subnet mask",
                    pcap_path=str(
                        Path("report_tc_network_pcap") / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "255.255.255.0"}]},
                    ],
                ),
                ANALYZER.pcap_checkFrames(
                    name="Validate DNS address",
                    pcap_path=str(
                        Path("report_tc_network_pcap") / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "8.8.8.8"}]},
                    ],
                ),
                ANALYZER.pcap_checkFrames(
                    name="Settings: Device Name present",
                    pcap_path=str(
                        Path("report_tc_network_pcap") / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "ENERGIS-1.0.0"}]},
                    ],
                ),
                ANALYZER.pcap_checkFrames(
                    name="Settings: Device Name present",
                    pcap_path=str(
                        Path("report_tc_network_pcap") / "capture_settings.pcap"
                    ),
                    display_filter=f"http.response and ip.addr=={hw.BASELINE_IP}",
                    ordered=False,
                    expected_frames=[
                        {"payload_patterns": [{"contains_ascii": "Location"}]},
                    ],
                ),
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
