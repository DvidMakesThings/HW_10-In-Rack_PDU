# TestCases/tc_network_eth.py
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
================================================================================
ENERGIS Ethernet & Web UI Functional Test Suite (UTFW)
--------------------------------------------------------------------------------
Universal test using UTFW.ethernet + UTFW.SNMP with detailed reporting.
All device- or route-specific details must be defined in hardware_config and
provided via --hwcfg "<path-to>/hardware_config.py".

Hardware protection: relay switch pacing enforced (max 1 switch / 200 ms).
Reboot-aware: network reconfiguration triggers device reboot; we tolerate the
POST disconnect and then wait for UART "SYSTEM READY" before probing HTTP.
================================================================================
"""

import sys

from UTFW.core import run_test_with_teardown
from UTFW.core import get_hwconfig
from UTFW.core import STE
from UTFW.modules import snmp as SNMP
from UTFW.modules import ethernet as ETH
from UTFW.modules import serial as UART

class tc_network_eth_test:
    """UTFW test suite for Ethernet/Web behavior (minimal WebUI, paced, reboot-aware)."""

    def __init__(self):
        pass

    def setup(self):
        """
        Build and return the ordered list of TestAction steps.

        Requires in hardware_config:
            BASELINE_IP (str)            e.g., "192.168.0.11"
            TEMP_NEW_IP (str)            e.g., "192.168.0.72"
            HTTP_PORT (int)              e.g., 80
            HTTP_TIMEOUT (float)         e.g., 3.0
            SERIAL_PORT (str)            e.g., "COM11" or "/dev/ttyACM0"
            OUTLET_BASE_OID (str)        e.g., "1.3.6.1.4.1.x.y.z"
            SNMP_COMMUNITY (str)         e.g., "public"
            NET_IP_OID (str)             OID string for current IP report via SNMP
            BASELINE_GATEWAY (str)
            BASELINE_SUBNET (str)
            BASELINE_DNS (str)
        """
        hw = get_hwconfig()

        base = f"http://{hw.BASELINE_IP}:{hw.HTTP_PORT}"
        new_base = f"http://{hw.TEMP_NEW_IP}:{hw.HTTP_PORT}"

        # Hardware protection: one relay switch operation at most every 0.2 s
        RELAY_PACE_KEY = "relay"
        RELAY_MIN_INTERVAL = 0.200  # seconds

        # HTTP dump files will be written under the active reports_dir/<DUMPS>
        DUMPS = "http_dumps"

        return [
            # 1) Reachability & HTTP sanity (minimal WebUI pages only)
            STE(
                ETH.ping_action(
                    name="Ping baseline",
                    ip=hw.BASELINE_IP,
                    count=1,
                    timeout_per_pkt=1
                ),
                ETH.http_get_action(
                    name="GET /",
                    base_url=base,
                    path="/",
                    timeout=hw.HTTP_TIMEOUT,
                    accept_status=(200, 304),
                    require_nonempty=True,
                    dump_subdir=DUMPS
                ),
                ETH.expect_header_prefix_action(
                    name="Root Content-Type is HTML",
                    base_url=base,
                    path="/",
                    header_name="Content-Type",
                    prefix="text/html",
                    timeout=hw.HTTP_TIMEOUT,
                    dump_subdir=DUMPS
                ),
                ETH.http_get_action(
                    name="GET /control.html",
                    base_url=base,
                    path="/control.html",
                    timeout=hw.HTTP_TIMEOUT,
                    accept_status=(200, 304),
                    require_nonempty=True,
                    dump_subdir=DUMPS
                ),
                ETH.http_get_action(
                    name="GET /settings.html",
                    base_url=base,
                    path="/settings.html",
                    timeout=hw.HTTP_TIMEOUT,
                    accept_status=(200, 304),
                    require_nonempty=True,
                    dump_subdir=DUMPS
                ),
                ETH.http_get_action(
                    name="GET /help.html",
                    base_url=base,
                    path="/help.html",
                    timeout=hw.HTTP_TIMEOUT,
                    accept_status=(200, 304),
                    require_nonempty=True,
                    dump_subdir=DUMPS
                ),
                name="Reachability/HTTP sanity"
            ),

            # 2) Outlet control via HTTP form + SNMP verify
            STE(
                *[
                    action
                    for ch in range(1, 9)
                    for action in (
                        # CHn ON
                        ETH.http_post_form_action(
                            name=f"HTTP CH{ch}=ON",
                            base_url=base,
                            path="/control",
                            form={f"channel{ch}": "on"},
                            timeout=hw.HTTP_TIMEOUT,
                            dump_subdir=DUMPS,
                            pace_key=RELAY_PACE_KEY, min_interval_s=RELAY_MIN_INTERVAL
                        ),
                        SNMP.get_outlet(
                            name=f"SNMP verify CH{ch}=1",
                            ip=hw.BASELINE_IP,
                            channel=ch,
                            expected_state=True,
                            outlet_base_oid=hw.OUTLET_BASE_OID,
                            community=hw.SNMP_COMMUNITY
                        ),
                        # CHn OFF (empty form = all off by handler default, we still verify per channel)
                        ETH.http_post_form_action(
                            name=f"HTTP CH{ch}=OFF",
                            base_url=base,
                            path="/control",
                            form={},
                            timeout=hw.HTTP_TIMEOUT,
                            dump_subdir=DUMPS,
                            pace_key=RELAY_PACE_KEY, min_interval_s=RELAY_MIN_INTERVAL
                        ),
                        SNMP.get_outlet(
                            name=f"SNMP verify CH{ch}=0",
                            ip=hw.BASELINE_IP,
                            channel=ch,
                            expected_state=False,
                            outlet_base_oid=hw.OUTLET_BASE_OID,
                            community=hw.SNMP_COMMUNITY
                        ),
                    )
                ],
                name="Per-channel ON/OFF + SNMP verify"
            ),

            # 3) ALL ON / ALL OFF 
            STE(
                ETH.http_post_form_action(
                    name="HTTP ALL ON",
                    base_url=base,
                    path="/control",
                    form={f"channel{i}": "on" for i in range(1, 9)},
                    timeout=hw.HTTP_TIMEOUT,
                    dump_subdir=DUMPS,
                    pace_key=RELAY_PACE_KEY, min_interval_s=RELAY_MIN_INTERVAL
                ),
                SNMP.verify_all_outlets(
                    name="SNMP verify all=ON",
                    ip=hw.BASELINE_IP,
                    expected_state=True,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    community=hw.SNMP_COMMUNITY
                ),
                ETH.http_post_form_action(
                    name="HTTP ALL OFF",
                    base_url=base,
                    path="/control",
                    form={},
                    timeout=hw.HTTP_TIMEOUT,
                    dump_subdir=DUMPS,
                    pace_key=RELAY_PACE_KEY, min_interval_s=RELAY_MIN_INTERVAL
                ),
                SNMP.verify_all_outlets(
                    name="SNMP verify all=OFF",
                    ip=hw.BASELINE_IP,
                    expected_state=False,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    community=hw.SNMP_COMMUNITY
                ),
                name="ALL ON/OFF + SNMP verify"
            ),

            # 4) Network configuration change -> TEMP_NEW_IP, then revert to baseline 
            STE(
                # Change IP (device replies 204 then reboots)
                ETH.http_post_form_action(
                    name="POST /settings (IP=new)",
                    base_url=base,
                    path="/settings",
                    form={
                        "ip": hw.TEMP_NEW_IP,
                        "gateway": hw.BASELINE_GATEWAY,
                        "subnet": hw.BASELINE_SUBNET,
                        "dns": hw.BASELINE_DNS
                    },
                    timeout=hw.HTTP_TIMEOUT,
                    dump_subdir=DUMPS,
                    tolerate_disconnect=True
                ),
                UART.wait_for_reboot(
                    name="Wait UART SYSTEM READY (new)",
                    port=hw.SERIAL_PORT,
                    baudrate=115200,
                    timeout=15.0
                ),
                ETH.wait_http_ready_action(
                    name="Wait HTTP ready (new IP)",
                    base_url=new_base,
                    path="/",
                    timeout_total=12.0
                ),
                SNMP.expect_oid_equals(
                    name="SNMP NET IP=new",
                    ip=hw.TEMP_NEW_IP,
                    oid=hw.NET_IP_OID,
                    expected=hw.TEMP_NEW_IP,
                    community=hw.SNMP_COMMUNITY
                ),

                # Revert to baseline
                ETH.http_post_form_action(
                    name="POST /settings (revert baseline)",
                    base_url=new_base,
                    path="/settings",
                    form={
                        "ip": hw.BASELINE_IP,
                        "gateway": hw.BASELINE_GATEWAY,
                        "subnet": hw.BASELINE_SUBNET,
                        "dns": hw.BASELINE_DNS
                    },
                    timeout=hw.HTTP_TIMEOUT,
                    dump_subdir=DUMPS,
                    tolerate_disconnect=True
                ),
                UART.wait_for_reboot(
                    name="Wait UART SYSTEM READY (baseline)",
                    port=hw.SERIAL_PORT,
                    baudrate=115200,
                    timeout=15.0
                ),
                ETH.wait_http_ready_action(
                    name="Wait HTTP ready (baseline IP)",
                    base_url=base,
                    path="/",
                    timeout_total=12.0
                ),
                SNMP.expect_oid_equals(
                    name="SNMP NET IP=baseline",
                    ip=hw.BASELINE_IP,
                    oid=hw.NET_IP_OID,
                    expected=hw.BASELINE_IP,
                    community=hw.SNMP_COMMUNITY
                ),
                name="Network configuration change + revert"
            ),

            # 5) Error path → unknown path returns 200
            STE(
                ETH.http_get_action(
                    name="GET unknown -> 200",
                    base_url=base,
                    path="/this-path-should-not-exist",
                    timeout=hw.HTTP_TIMEOUT,
                    accept_status=(200,),
                    require_nonempty=True,
                    dump_subdir=DUMPS
                ),
                ETH.expect_header_prefix_action(
                    name="Unknown path Content-Type HTML",
                    base_url=base,
                    path="/this-path-should-not-exist",
                    header_name="Content-Type",
                    prefix="text/html",
                    timeout=hw.HTTP_TIMEOUT,
                    dump_subdir=DUMPS
                ),
                name="Error path behavior"
            ),

            # 6) Teardown to initial All-off state
            STE(
                ETH.http_post_form_action(
                    name="Final ALL OFF",
                    base_url=base,
                    path="/control",
                    form={},
                    timeout=hw.HTTP_TIMEOUT,
                    dump_subdir=DUMPS,
                    pace_key=RELAY_PACE_KEY, min_interval_s=RELAY_MIN_INTERVAL
                ),
                SNMP.verify_all_outlets(
                    name="Verify final all=OFF",
                    ip=hw.BASELINE_IP,
                    expected_state=False,
                    outlet_base_oid=hw.OUTLET_BASE_OID,
                    community=hw.SNMP_COMMUNITY
                ),
                name="Teardown to initial All-off state"
            ),
        ]


def main():
    test_instance = tc_network_eth_test()
    return run_test_with_teardown(
        test_class_instance=test_instance,
        test_name="tc_network_eth",
        reports_dir="report_tc_network_eth"
    )


if __name__ == "__main__":
    sys.exit(main())