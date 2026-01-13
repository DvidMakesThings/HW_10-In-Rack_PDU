"""
Energis error-code name tables.

Split out from viewFault.py to keep the main app modular.
This file contains:
- MODULE_NAMES
- SEVERITY_NAMES
- FID_NAMES

Source: ERROR_CODE.h in the Energis firmware.
"""

# Module IDs from ERROR_CODE.h (ERR_MOD_*)
MODULE_NAMES = {
    0x1: "INIT",
    0x2: "NET",
    0x3: "METER",
    0x4: "STORAGE",
    0x5: "BUTTON",
    0x6: "HEALTH",
    0x7: "LOGGER",
    0x8: "CONSOLE",
    0x9: "OCP",
    0xA: "SWITCH",
}

# Severities from error_severity_t
SEVERITY_NAMES = {
    0x1: "INFO",
    0x2: "WARNING",
    0x4: "ERROR",
    0xF: "FATAL",
}

# File IDs per module, from ERROR_CODE.h (ERR_FID_*)
FID_NAMES = {
    0x1: {  # INIT
        0x0: "energis_rtos.c",
        0xF: "inittask.c",
    },
    0x2: {  # NET
        0x0: "control_handler.c",
        0x1: "http_server.c",
        0x2: "metrics_handler.c",
        0x3: "page_content.c",
        0x4: "settings_handler.c",
        0x5: "status_handler.c",
        0x6: "snmp_custom.c",
        0x7: "snmp_networkctrl.c",
        0x8: "snmp_outletctrl.c",
        0x9: "snmp_powermon.c",
        0xA: "snmp_voltagemon.c",
        0xB: "ethernet_driver.c",
        0xC: "socket.c",
        0xD: "socket.c",
        0xE: "snmp.c",
        0xF: "nettask.c",
    },
    0x3: {  # METER
        0x0: "hlw8032_driver.c",
        0xF: "metertask.c",
    },
    0x4: {  # STORAGE
        0x0: "calibration.c",
        0x1: "channel_labels.c",
        0x2: "energy_monitor.c",
        0x3: "event_log.c",
        0x4: "factory_defaults.c",
        0x5: "cat24c256_driver.c",
        0x6: "network.c",
        0x7: "storage_common.c",
        0x8: "user_output.c",
        0x9: "user_prefs.c",
        0xA: "device_identity.c",
        0xF: "storagetask.c",
    },
    0x5: {  # BUTTON
        0x0: "button_driver.c",
        0x1: "mcp23017_driver.c",
        0x2: "mcp23017_driver.c",
        0xF: "buttontask.c",
    },
    0x6: {  # HEALTH
        0x0: "crashlog.c",
        0x1: "power_mgr.c",
        0x2: "rtos_hooks.c",
        0x3: "wrap_watchdog.c",
        0xF: "healthtask.c",
    },
    0x7: {  # LOGGER
        0x0: "helpers.c",
        0xF: "loggertask.c",
    },
    0x8: {  # CONSOLE
        0x0: "consoletask.c",
        0x1: "consoletask.c",
        0x2: "consoletask.c",
        0x3: "provisioning_commands.c",
    },
    0x9: {  # OCP
        0xF: "ocp.c",
    },
    0xA: {  # SWITCH
        0x0: "switchtask.c",
        0x1: "switchtask.c",
        0x2: "switchtask.c",
        0x3: "switchtask.c",
        0x4: "switchtask.c",
        0x5: "switchtask.c",
    },
}
