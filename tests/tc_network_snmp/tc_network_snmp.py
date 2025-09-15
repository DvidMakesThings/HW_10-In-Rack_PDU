#!/usr/bin/env python3
"""
ENERGIS SNMP Functional Test (UTFW)
===================================

Reimplementation of the legacy `tc_network_snmp.py` in the UTFW framework style.

- Uses UTFW's TestAction list returned by `setup()`.
- Validates enterprise walk presence, MIB-II system group, long-length OIDs,
  network config OIDs, per-channel outlet control (SNMP) with UART verification,
  and ALL ON/OFF triggers with verification.

Place under: <project>/TestCases/tc_network_snmp/tc_network_snmp_utfw.py
"""

import sys
import subprocess
from pathlib import Path

from UTFW.core import run_test_with_teardown
from UTFW.core import get_hwconfig
from UTFW.core import STE
from UTFW.modules import snmp as SNMP
from UTFW.modules import serial

class tc_network_snmp_test:
    """UTFW test suite for ENERGIS SNMP behavior."""

    def __init__(self):
        pass

    def setup(self):
        """Assemble and return ordered TestAction list."""
        hw = get_hwconfig()

        ip = hw.BASELINE_IP
        comm = hw.SNMP_COMMUNITY

        # Step 2: MIB-II system.* regex validations
        validate_MIB_II = STE(
            # Step 2.1 - sysDescr
            SNMP.expect_oid_regex(
                name="sysDescr matches expected",
                ip=hw.BASELINE_IP,
                oid=hw.SYS_DESCR,
                regex=hw.SYS_DESCR_EXPECTED,
                community=hw.SNMP_COMMUNITY
            ),
            # Step 2.2 - sysObjectID
            SNMP.expect_oid_regex(
                name="sysObjectID present",
                ip=hw.BASELINE_IP,
                oid=hw.SYS_OBJID,
                regex=r".+",
                community=hw.SNMP_COMMUNITY
            ),
            # Step 2.3 - sysUpTime
            SNMP.expect_oid_regex(
                name="sysUpTime present",
                ip=hw.BASELINE_IP,
                oid=hw.SYS_UPTIME,
                regex=r".*\d+.*",
                community=hw.SNMP_COMMUNITY
            ),
            # Step 2.4 - sysContact
            SNMP.expect_oid_regex(
                name="sysContact matches expected",
                ip=hw.BASELINE_IP,
                oid=hw.SYS_CONTACT,
                regex=hw.SYS_CONTACT_EXPECTED,
                community=hw.SNMP_COMMUNITY
            ),
            # Step 2.5 - sysName
            SNMP.expect_oid_regex(
                name="sysName matches expected",
                ip=hw.BASELINE_IP,
                oid=hw.SYS_NAME,
                regex=hw.SYS_NAME_EXPECTED,
                community=hw.SNMP_COMMUNITY
            ),
            # Step 2.6 - sysLocation
            SNMP.expect_oid_regex(
                name="sysLocation matches expected",
                ip=hw.BASELINE_IP,
                oid=hw.SYS_LOCATION,
                regex=hw.SYS_LOCATION_EXPECTED,
                community=hw.SNMP_COMMUNITY
            ),
            # Step 2.7 - sysServices
            SNMP.expect_oid_regex(
                name="sysServices matches expected",
                ip=hw.BASELINE_IP,
                oid=hw.SYS_SERVICES,
                regex=hw.SYS_SERVICES_EXPECTED,
                community=hw.SNMP_COMMUNITY
            ),
            name="Validate MIB-II system group"
        )

        # Step 3: Long-length test entries (one OK, one expected error)
        longlength_test = STE(
            SNMP.expect_oid_regex(
                name="19865.1.0 long-length string OK",
                ip=hw.BASELINE_IP,
                oid=hw.LONG_LENGTH_TEST_1,
                regex=hw.LONG_LENGTH_1_EXPECTED,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.expect_oid_error(
                name="19865.2.0 produces noSuchName (expected error)",
                ip=hw.BASELINE_IP,
                oid=hw.LONG_LENGTH_TEST_2,
                community=hw.SNMP_COMMUNITY
            ),
            name="Validate long-length OIDs"
        )

        # Step 4: Network configuration OIDs equal to expected values
        test_network_config = STE(
            SNMP.expect_oid_equals(
                name="Network IP equals baseline",
                ip=hw.BASELINE_IP,
                oid=hw.NET_IP_OID,
                expected=hw.BASELINE_IP,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.expect_oid_equals(
                name="Network SN equals expected",
                ip=hw.BASELINE_IP,
                oid=hw.NET_SN_OID,
                expected=hw.BASELINE_SUBNET,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.expect_oid_equals(
                name="Network GW equals expected",
                ip=hw.BASELINE_IP,
                oid=hw.NET_GW_OID,
                expected=hw.BASELINE_GATEWAY,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.expect_oid_equals(
                name="Network DNS equals expected",
                ip=hw.BASELINE_IP,
                oid=hw.NET_DNS_OID,
                expected=hw.BASELINE_DNS,
                community=hw.SNMP_COMMUNITY
            ),
            name="Validate network configuration OIDs"
        )

        # Step 6: ALL ON / ALL OFF using trigger OIDs + verify 1..8
        test_all_outlets = STE(
            SNMP.set_all_outlets(
                name="ALL ON trigger",
                ip=hw.BASELINE_IP,
                all_on_oid=hw.ALL_ON_OID,
                all_off_oid=hw.ALL_OFF_OID,
                state=True,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.verify_all_outlets(
                name="Verify ALL ON via SNMP (1..8 == 1)",
                ip=hw.BASELINE_IP,
                outlet_base_oid=hw.OUTLET_BASE_OID,
                expected_state=True,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.set_all_outlets(
                name="ALL ON trigger",
                ip=hw.BASELINE_IP,
                all_on_oid=hw.ALL_ON_OID,
                all_off_oid=hw.ALL_OFF_OID,
                state=False,
                community=hw.SNMP_COMMUNITY
            ),
            SNMP.verify_all_outlets(
                name="Verify ALL OFF via SNMP (1..8 == 0)",
                ip=hw.BASELINE_IP,
                outlet_base_oid=hw.OUTLET_BASE_OID,
                expected_state=False,
                community=hw.SNMP_COMMUNITY
            ),
            name="Cycle ALL ON / ALL OFF with SNMP verify"
        )

        return [
            # Step 1: Walk enterprise subtree
            SNMP.walk_enterprise(
                name="Walk enterprise subtree",
                ip=hw.BASELINE_IP,
                community=hw.SNMP_COMMUNITY,
                root_oid=hw.ENTERPRISE_OID
            ),

            # Step 2: MIB-II system.* regex validations
            validate_MIB_II,

            # Step 3: Long-length OID validation
            longlength_test,

            # Step 4: Validate network configuration
            test_network_config,

            # Step 5: Per-channel outlet set/verify (SNMP + UART)
            SNMP.cycle_outlets_all_channels(
                name="Cycle all channels ON/OFF with SNMP verify",
                ip=hw.BASELINE_IP,
                outlet_base_oid=hw.OUTLET_BASE_OID,
                community=hw.SNMP_COMMUNITY,
                settle_s=0.5
            ),

            # Step 6: Validate all outlets (SNMP + UART)
            test_all_outlets

        ]


def main():
    """Create test instance and run"""
    test_instance = tc_network_snmp_test()
    return run_test_with_teardown(
        test_class_instance=test_instance, 
        test_name="tc_network_snmp",
        reports_dir="report_tc_network_snmp"
    )


if __name__ == "__main__":
    sys.exit(main())

