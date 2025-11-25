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
        0x0: "ENERGIS_RTOS.c",
        0xF: "InitTask.c",
    },
    0x2: {  # NET
        0x0: "control_handler.c",
        0x1: "http_server.c",
        0x2: "metrics_handler.c",
        0x3: "page_content.c",
        0x4: "settings_handler.c",
        0x5: "status_handler.c",
        0x6: "snmp_custom.c",
        0x7: "snmp_networkCtrl.c",
        0x8: "snmp_outletCtrl.c",
        0x9: "snmp_powerMon.c",
        0xA: "snmp_voltageMon.c",
        0xB: "ethernet_driver.c",
        0xC: "socket.c",
        0xD: "socket.c",
        0xE: "drivers/snmp.c",
        0xF: "NetTask.c",
    },
    0x3: {  # METER
        0x0: "HLW8032_driver.c",
        0xF: "MeterTask.c",
    },
    0x4: {  # STORAGE
        0x0: "calibration.c",
        0x1: "channel_labels.c",
        0x2: "energy_monitor.c",
        0x3: "event_log.c",
        0x4: "factory_defaults.c",
        0x5: "CAT24C256_driver.c",
        0x6: "network.c",
        0x7: "storage_common.c",
        0x8: "user_output.c",
        0x9: "user_prefs.c",
        0xF: "StorageTask.c",
    },
    0x5: {  # BUTTON
        0x0: "button_driver.c",
        0x1: "MCP23017_driver.c",
        0x2: "MCP23017_driver.c",
        0xF: "ButtonTask.c",
    },
    0x6: {  # HEALTH
        0x0: "crashlog.c",
        0x1: "power_mgr.c",
        0x2: "rtos_hooks.c",
        0x3: "wrap_watchdog.c",
        0xF: "HealthTask.c",
    },
    0x7: {  # LOGGER
        0x0: "helpers.c",
        0xF: "LoggerTask.c",
    },
    0x8: {  # CONSOLE
        0x0: "ConsoleTask.c",
        0x1: "ConsoleTask.c",
        0x2: "ConsoleTask.c",
    },
}
