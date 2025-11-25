"""
Friendly descriptions for Energis error codes, keyed by module -> fid -> severity -> eid.

Structure: EID_NAMES[module][fid][severity][eid] = "meaning"

This accounts for the fact that the same EID can have different meanings
depending on the severity level (ERROR vs WARNING vs FATAL).
"""

# EID descriptions: EID_NAMES[module][fid][severity][eid] = "meaning"
EID_NAMES = {
    0x1: {  # 1
        0x0: {
            0xF: {  # FATAL
                0x0: "Scheduler returned - should never happen!",
            },
        },
        0xF: {
            0x4: {  # ERROR
                0x0: "Some MCP23017s missing!",
                0x1: "LoggerTask NOT ready (timeout)",
                0x2: "ConsoleTask NOT ready (timeout)",
                0x3: "StorageTask not ready (timeout)",
                0x4: "ButtonTask NOT ready (timeout)",
                0x5: "NetTask not ready (timeout)",
                0x6: "Failed to create MeterTask",
                0x7: "12V rail low, waiting... / MeterTask not ready (timeout)",
                0x8: "Storage config NOT ready (timeout)",
                0x9: "Meter not ready after wait; NOT starting Health.",
            },
        },
    },
    0x2: {  # 2
        0x1: {
            0x4: {  # ERROR
                0x0: "Send failed on sock",
                0x3: "HTTP buffer allocation failed",
                0x4: "HTTP socket open failed",
                0x5: "HTTP listen failed",
            },
        },
        0x2: {
            0x4: {  # ERROR
                0x0: "Metrics buffer overflow on energis_up",
                0x1: "Metrics buffer overflow on energis_build_info",
                0x2: "Metrics buffer overflow on energis_uptime_seconds_total",
                0x3: "Metrics buffer overflow on energis_internal_temperature_celsius",
                0x4: "Metrics buffer overflow on energis_temp_calibrated",
                0x5: "Metrics buffer overflow on energis_temp_calibration_mode",
                0x6: "Metrics buffer overflow on energis_vusb_volts",
                0x7: "Metrics buffer overflow on energis_vsupply_volts",
                0x8: "Metrics buffer overflow on energis_http_requests_total",
                0x9: "Metrics buffer overflow on energis_channel_state",
                0xA: "Metrics buffer overflow on energis_channel_telemetry_valid",
                0xB: "Metrics buffer overflow on energis_channel_voltage_volts",
                0xC: "Metrics buffer overflow on energis_channel_current_amps",
                0xD: "Metrics buffer overflow on energis_channel_power_watts",
                0xE: "Metrics buffer overflow on energis_channel_energy_watt_hours_total",
                0xF: "Metrics buffer overflow during metrics render: 503",
            },
        },
        0x4: {
            0x4: {  # ERROR
                0x1: "Missing POST body in settings handler",
            },
        },
        0x7: {
            0x4: {  # ERROR
                0x1: "Null pointer in load netconfig",
            },
        },
        0xB: {
            0x2: {  # WARNING
                0x0: "PHY link timeout",
            },
            0x4: {  # ERROR
                0x1: "Null pointer or zero length in ethernet receive",
                0x2: "Zero length in ethernet receive ignore",
                0x3: "Failed to create SPI mutex",
                0x4: "NULL network info",
                0x5: "",
                0x6: "Invalid ethernet controller version",
                0x7: "NULL network info: Network config not applied",
                0x8: "NULL network info: Cannot read network config",
                0x9: "NULL network info: Cannot print network config",
                0xA: "NULL PHY configuration, PHY config not written",
                0xB: "NULL PHY configuration, PHY config not returned",
            },
        },
        0xC: {
            0x2: {  # WARNING
                0x6: "Disconnect fail, Non-blocking mode not supported",
            },
            0x4: {  # ERROR
                0x1: "TCP requested but no IP configured",
                0x2: "Invalid flag bit set (0x04 reserved)",
                0x3: "Invalid TCP flags (only NODELAY/NONBLOCK allowed)",
                0x4: "IGMPv2 set without MULTI_ENABLE",
                0x5: "UNIBLOCK set without MULTI_ENABLE",
                0x7: "Disconnect timeout",
                0x8: "Listen failed, socket moved to invalid state 0x",
                0x9: "Listen timeout",
                0xA: "Socket not connected",
                0xB: "Send timeout, socket closed",
                0xC: "Socket dropped while waiting for TX space",
                0xD: "Socket disconnected during TX wait",
                0xE: "TX wait timeout (5s)",
                0xF: "Socket not connected",
            },
        },
        0xD: {
            0x4: {  # ERROR
                0x1: "Socket dropped while waiting for RX data",
                0x4: "Socket not in UDP mode",
                0x5: "Socket left UDP mode while waiting for TX space",
                0x6: "Socket not in UDP mode",
                0x7: "Socket MACRAW packet too large",
                0x8: "Invalid I/O mode",
                0x9: "Invalid control type",
                0xA: "Keep-alive auto must be disabled before sending keep-alive",
                0xB: "Keep-alive send timeout on socket",
                0xC: "Invalid socket option",
                0xD: "PACKINFO not valid for TCP sockets",
                0xE: "Invalid socket option",
            },
        },
        0xE: {
            0x1: {  # INFO
                0x0: "OID not found for VarBind index",
            },
            0x4: {  # ERROR
                0x0: "Agent UDP socket open failed / OID not found",
                0x1: "Unsupported SNMP data type",
                0x2: "Set value type mismatch",
                0x3: "VarBind missing OID",
                0x4: "VarBind not a SEQUENCE",
                0x5: "VarBindList not SEQUENCE OF",
                0x6: "Inner VarBind parse failed",
                0x7: "Invalid SNMP PDU type",
                0x8: "VarBindList parse failed",
                0x9: "Invalid community length",
                0xA: "SNMP request parse failed",
                0xB: "Unauthorized community",
                0xC: "Unsupported SNMP version",
                0xD: "Failed to parse community string",
                0xE: "Invalid SNMP message header",
                0xF: "Failed to parse SNMP version",
            },
        },
        0xF: {
            0x2: {  # WARNING
                0x1: "Network configuration failed link was up, W5500 reinit from cached config failed",
                0x3: "waiting for config ready.",
                0x4: "using fallback network defaults",
                0x5: "No PHY link at startup",
                0x6: "PHY link DOWN detected",
            },
            0x4: {  # ERROR
                0x1: "w5500_hw_init failed",
                0x2: "SNMP init failed",
                0x3: "Network configuration failed to read from EEPROM",
                0x4: "Ethernet HW init failed, entering safe loop",
                0x5: "Failed to create Net task",
            },
        },
    },
    0x3: {  # 3
        0x0: {
            0x4: {  # ERROR
                0x1: "Invalid channel",
                0x2: "Invalid channel for uptime",
                0x3: "Invalid channel",
            },
        },
        0xF: {
            0x2: {  # WARNING
                0x1: "Two Point Compute: computed slope out of range",
                0x2: "Two Point Compute: computed slope out of range",
                0x3: "SetTempCalibration: slope out of range",
                0x4: "SetTempCalibration: V0 out of range",
            },
            0x4: {  # ERROR
                0x2: "Failed to create telemetry queue",
                0x3: "NULL pointer in MeterTask_GetSystemTelemetry",
                0x4: "NULL pointer in Single Point Compute",
                0x5: "CRITICAL: USB supply low",
                0x6: "CRITICAL: 12V supply low",
                0x7: "CRITICAL: Die temperature high",
                0x8: "CRITICAL: Die temperature low",
            },
        },
    },
    0x4: {  # 4
        0x0: {
            0x4: {  # ERROR
                0x1: "Write Sensor Calibration: Length exceeds max",
                0x2: "Read Sensor Calibration: Length exceeds max",
                0x3: "Write Sensor Calibration: Invalid channel / Read Sensor Calibration: Invalid channel",
                0x4: "Null pointer was passed to write calibration data",
                0x5: "Calibration size exceeds max",
                0x6: "Null pointer was passed to read calibration data",
                0x7: "Read Temperature calibration: Invalid calibration data,",
                0x8: "Compute Single Point: Null pointer",
                0x9: "Compute Two Point: Null pointer",
                0xA: "Compute Two Point: Identical temperature points",
                0xB: "Computed two point slope out of range",
                0xC: "Computed two point V0 out of range",
                0xD: "Apply To MeterTask: Null pointer",
                0xE: "Apply To MeterTask: Invalid calibration data",
            },
        },
        0x1: {
            0x4: {  # ERROR
                0x0: "Invalid Label input to write",
                0x1: "Invalid Label input to read",
                0x2: "Invalid Label input to clear",
                0x3: "Clear all failed",
            },
        },
        0x2: {
            0x4: {  # ERROR
                0x1: "Write length exceeds size",
                0x2: "Read length exceeds size",
                0x3: "Failed to write energy record",
            },
        },
        0x4: {
            0x2: {  # WARNING
                0x1: "Serial Number mismatch",
                0x2: "Network Configuration CRC mismatch",
                0x3: "Sensor Calibration read error",
            },
            0x4: {  # ERROR
                0x1: "Factory defaults write encountered errors",
                0x2: "Factory defaults write failed",
            },
        },
        0x5: {
            0x4: {  # ERROR
                0x0: "Byte write failed",
                0x1: "Byte read failed",
                0x2: "Buffer write failed: NULL data pointer",
                0x3: "Buffer write failed",
                0x4: "Buffer read failed: NULL buffer pointer",
                0x5: "Buffer read failed",
                0x6: "Self-test buffer write failed",
                0x7: "Self-test failed: Data mismatch",
            },
        },
        0x6: {
            0x2: {  # WARNING
                0x1: "Network MAC address was corrupted",
                0x2: "Failed to read valid network config from EEPROM",
            },
            0x4: {  # ERROR
                0x1: "Sysinfo write length exceeds size",
                0x2: "Sysinfo read length exceeds size",
                0x3: "Sysinfo write length exceeds size",
                0x4: "Sysinfo read length exceeds size",
                0x5: "CRC mismatch ",
                0x6: "Sysinfo write length exceeds size",
                0x7: "Sysinfo read length exceeds size",
                0x8: "Null pointer provided to Network config write",
                0x9: "Null pointer provided to Network config read",
                0xA: "CRC mismatch",
            },
        },
        0x8: {
            0x4: {  # ERROR
                0x1: "Write length exceeds size",
                0x2: "Read length exceeds size",
            },
        },
        0x9: {
            0x2: {  # WARNING
                0x0: "Using default user prefs due to read/CRC failure",
            },
            0x4: {  # ERROR
                0x1: "Write length exceeds size",
                0x2: "Read length exceeds size",
                0x3: "Null pointer for user prefs",
                0x4: "Null pointer for user prefs",
                0x5: "User prefs CRC mismatch",
            },
        },
    },
    0x5: {  # 5
        0x0: {
            0x4: {  # ERROR
                0x1: "MCP23017 selection device not found",
                0x2: "NULL io_index pointer",
                0x3: "NULL io_index pointer",
                0x4: "MCP23017 relay device not found",
            },
        },
        0x1: {
            0x4: {  # ERROR
                0x1: "I2C write fail ",
                0x2: "I2C read fail, NULL output",
                0x3: "I2C read fail",
                0x4: "Register bad args",
                0x5: "Registry full",
                0x6: "Mutex create failed",
                0x7: "Cannot initialize. NULL device pointer",
                0x8: "Device already initialized",
                0x9: "Cannot write register. NULL device pointer",
                0xA: "Cannot read register. NULL device or output pointer",
                0xB: "Cannot set direction. NULL device or bad pin",
                0xC: "Cannot write pin. NULL device or bad pin",
                0xD: "Cannot read pin. NULL device or bad pin",
                0xE: "Cannot write mask. NULL device pointer",
                0xF: "Cannot resync. NULL device pointer",
            },
        },
        0x2: {
            0x4: {  # ERROR
                0x1: "Cannot set channel state. bad channel",
                0x2: "Cannot set channel state. MCP23017 relay device not found",
                0x3: "Cannot get channel state. bad channel",
                0x4: "Cannot get channel state. MCP23017 relay device not found",
                0x5: "Error cannot set. MCP23017 display device not found",
                0x6: "Power good cannot set. MCP23017 display device not found",
                0x7: "Network link cannot set. MCP23017 display device not found",
            },
        },
        0xF: {
            0x4: {  # ERROR
                0x1: "Storage not ready, start timeout",
                0x2: "Button event queue create failed",
                0x3: "Blink timer create failed",
                0x4: "Button task create failed",
            },
        },
    },
    0x6: {  # 6
        0x1: {
            0x2: {  # WARNING
                0x0: "Already in STANDBY",
                0x1: "Already in RUN mode",
            },
        },
        0x2: {
            0xF: {  # FATAL
                0x0: "HardFault (CPU exception)",
            },
        },
        0x3: {
            0xF: {  # FATAL
                0x0: "log_err buffer truncated",
                0x1: "Panic msg",
                0x2: "Hard assert failure",
                0x3: "Invalid params in",
                0x4: "reset_usb_boot called",
                0x5: "runtime_unreset_core called",
                0x6: "runtime_reboot called",
                0x7: "__NVIC_SystemReset called",
                0x8: "scb_reboot called",
            },
        },
        0xF: {
            0x1: {  # INFO
                0x0: "INTENTIONAL REBOOT",
                0x1: "GRACE PERIOD ENDED",
                0x4: "Error in HealthTask.c",
            },
            0x2: {  # WARNING
                0x0: "scheduler idle stalled ~u ms (idle_last_ms=u)",
                0x2: "Error in HealthTask.c",
                0x3: "Misbehaving: NEVER heartbeated",
                0x4: "Misbehaving task detected",
                0x5: "Re-register on ID = Name = (Keeping last_seen_ms = u)",
            },
            0xF: {  # FATAL
                0x1: "Watchdog PreBarked",
                0x2: "Stale task detected",
                0x5: "Health task called PreBark",
                0x6: "WDT after-reboot report emitted",
                0x7: "WDT after-reboot stale task report emitted",
            },
        },
    },
    0x7: {  # 7
        0x0: {
            0x4: {  # ERROR
                0x1: "Null networkInfo pointer",
                0x2: "\"Last fault\" reported from watchdog scratch",
            },
        },
        0xF: {
            0x4: {  # ERROR
                0x0: "Failed to create logger queue",
            },
        },
    },
    0x8: {  # 8
        0x0: {
            0x2: {  # WARNING
                0x0: "Invalid arguments for CALIBRATE. Usage: CALIBRATE <ch> <voltage> <current>",
            },
            0x4: {  # ERROR
                0x0: "Invalid IP address format",
                0x1: "Invalid IP address octet value",
                0x2: "Invalid channel for HLW8032 read",
                0x3: "EEPROM busy",
                0x4: "EEPROM write failed during 1-Point temperature calibration",
                0x5: "EEPROM write failed during 2-Point temperature calibration",
                0x6: "Invalid SET_CH arguments",
                0x7: "Invalid value for SET_CH state",
                0x8: "Failed to set CH during SET_CH ALL",
                0x9: "Invalid channel for SET_CH",
                0xA: "Failed to set CH during SET_CH",
                0xB: "Missing arguments for GET_CH",
                0xC: "Invalid channel for GET_CH",
                0xD: "Invalid channel for CALIBRATE",
                0xE: "Negative reference values were given for CALIBRATE",
                0xF: "Calibration failed for a channel",
            },
        },
        0x1: {
            0x4: {  # ERROR
                0x0: "Auto zero-point calibration had failures",
                0x1: "Invalid reference voltage for AUTO_CAL_V",
                0x2: "Auto voltage calibration had failures",
                0x3: "Invalid channel for SHOW_CALIB",
                0x4: "Invalid IP address format",
                0x5: "Failed to read network config",
                0x6: "Failed to save IP address",
                0x7: "Invalid subnet mask format",
                0x8: "Failed to read network config",
                0x9: "Failed to save subnet mask",
                0xA: "Invalid gateway format",
                0xB: "Failed to read network config",
                0xC: "Failed to save gateway",
                0xD: "Invalid DNS format",
                0xE: "Failed to read network config",
                0xF: "Failed to save DNS",
            },
        },
        0x2: {
            0x4: {  # ERROR
                0x0: "Invalid CONFIG_NETWORK arguments",
                0x1: "Failed to read network config",
                0x2: "Invalid IP",
                0x3: "Invalid subnet",
                0x4: "Invalid gateway",
                0x5: "Invalid DNS",
                0x6: "Failed to save network config",
                0x7: "Failed to enqueue error log dump (storage busy)",
                0x8: "Failed to enqueue warning log dump (storage busy)",
                0x9: "Failed to enqueue error log clear (storage busy)",
                0xA: "Failed to enqueue warning log clear (storage busy)",
                0xB: "Queue creation failed",
                0xC: "Failed to create task",
            },
        },
    },
}