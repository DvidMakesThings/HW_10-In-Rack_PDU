"""
Friendly descriptions for Energis error codes, keyed by module -> fid -> severity -> eid.

Structure: EID_NAMES[module][fid][severity][eid] = "meaning"

This accounts for the fact that the same EID can have different meanings
depending on the severity level (ERROR vs WARNING vs FATAL).

ALL EIDs have been corrected to be sequential within each FID/severity combination.
Last updated: 2026-01-06
"""

# EID descriptions: EID_NAMES[module][fid][severity][eid] = "meaning"
EID_NAMES = {
    0x1: {  # INIT MODULE
        0x0: {  # energis_rtos.c
            0xF: {  # FATAL
                0x0: "Scheduler returned - should never happen!",
            },
        },
        0xF: {  # inittask.c
            0x4: {  # ERROR
                0x0: "Some MCP23017s missing!",
                0x1: "LoggerTask NOT ready (timeout)",
                0x2: "ConsoleTask NOT ready (timeout)",
                0x3: "StorageTask not ready (timeout)",
                0x4: "SwitchTask not ready (timeout)",
                0x5: "ButtonTask NOT ready (timeout)",
                0x6: "Failed to create NetTask",
                0x7: "NetTask not ready (timeout)",
                0x8: "Failed to create MeterTask",
                0x9: "MeterTask not ready (timeout)",
                0xA: "Storage config NOT ready (timeout)",
                0xB: "Meter not ready after wait; NOT starting Health",
                0xC: "12V rail low, waiting...",
            },
        },
    },
    0x2: {  # NET MODULE
        0x1: {  # http_server.c
            0x4: {  # ERROR
                0x0: "Send failed on socket",
                0x1: "HTTP buffer allocation failed",
                0x2: "HTTP socket open failed",
                0x3: "HTTP listen failed",
            },
        },
        0x2: {  # metrics_handler.c
            0x4: {  # ERROR
                0x0: "Metrics buffer overflow on energis_up",
                0x1: "Metrics buffer overflow on energis_build_info",
                0x2: "Metrics buffer overflow on energis_uptime_seconds_total",
                0x3: "Metrics buffer overflow on energis_mem_stats",
                0x4: "Metrics buffer overflow on energis_temp_calibrated",
                0x5: "Metrics buffer overflow on energis_temp_calibration_mode",
                0x6: "Metrics buffer overflow on energis_vusb_volts",
                0x7: "Metrics buffer overflow on energis_vsupply_volts",
                0x8: "Metrics buffer overflow on energis_http_requests_total",
                0x9: "Metrics buffer overflow on energis_channel_state",
                0xA: "Metrics buffer overflow on energis_channel_uptime",
                0xB: "Metrics buffer overflow on energis_channel_voltage_volts",
                0xC: "Metrics buffer overflow on energis_channel_current_amps",
                0xD: "Metrics buffer overflow on energis_channel_power_watts",
                0xE: "Metrics buffer overflow on energis_channel_energy_wh",
                0xF: "503 Buffer overflow during metrics render",
            },
        },
        0x4: {  # settings_handler.c
            0x4: {  # ERROR
                0x0: "Send failed on socket",
                0x1: "Missing POST body in settings handler",
            },
        },
        0x7: {  # snmp_networkctrl.c
            0x4: {  # ERROR
                0x1: "Null pointer in load netconfig",
            },
        },
        0x8: {  # snmp_outletctrl.c
            0x2: {  # WARNING
                0x0: "AllOn failed",
                0x1: "AllOff failed",
            },
            0x4: {  # ERROR
                0x1: "Invalid channel index",
                0x2: "Switch_SetChannelCompat failed",
            },
        },
        0xB: {  # ethernet_driver.c
            0x4: {  # ERROR
                0x0: "Null pointer or zero length in ethernet receive",
                0x1: "Zero length in Ethernet receive ignore",
                0x2: "Failed to create SPI mutex",
                0x3: "w5500_chip_init: NULL network info",
                0x4: "PHY link timeout",
                0x5: "Invalid ethernet controller version",
                0x6: "w5500_set_network: NULL network info - Network config not applied",
                0x7: "w5500_get_network: NULL network info - Cannot read network config",
                0x8: "w5500_print_network: NULL network info - Cannot print network config",
                0x9: "w5500_set_phy_conf: NULL PHY configuration - PHY config not written",
                0xA: "w5500_get_phy_conf: NULL PHY configuration - PHY config not returned",
            },
        },
        0xC: {  # socket.c
            0x2: {  # WARNING
                0x6: "Disconnect fail, Non-blocking mode not supported",
            },
            0x4: {  # ERROR
                0x1: "TCP requested but no IP configured",
                0x2: "Invalid flag bit set (0x04 reserved)",
                0x3: "Socket open failed",
                0x4: "Socket open timeout",
                0x5: "Socket open failed after retry",
                0x7: "Disconnect timeout",
                0x8: "Listen failed",
                0x9: "Listen timeout",
                0xA: "Socket not connected during send",
                0xB: "Socket send timeout",
                0xC: "Socket not connected during send",
                0xD: "Socket not connected during send",
                0xE: "Socket send wait timeout",
                0xF: "Socket not connected during recv",
            },
        },
        0xD: {  # socket.c (second FID)
            0x4: {  # ERROR
                0x0: "Socket not connected during recv",
                0x1: "Socket not in UDP mode (sendto)",
                0x2: "Socket not in UDP mode (sendto state check)",
                0x3: "Socket not in UDP mode (recvfrom)",
                0x4: "Socket MACRAW packet too large",
                0x5: "ctlsocket: Invalid I/O mode",
                0x6: "ctlsocket: Invalid control type",
                0x7: "setsockopt: Keep-alive auto must be disabled before sending keep-alive",
                0x8: "setsockopt: Keep-alive send timeout on socket",
                0x9: "setsockopt: Invalid socket option",
                0xA: "getsockopt: PACKINFO not valid for TCP sockets",
                0xB: "getsockopt: Invalid socket option",
            },
        },
        0xE: {  # snmp.c
            0x1: {  # INFO
                0x0: "parseVarBind: OID not found for VarBind index",
            },
            0x4: {  # ERROR
                0x0: "SNMP agent: Failed to open UDP socket",
                0x1: "getEntry: Unsupported SNMP data type",
                0x2: "setEntry: Data type mismatch",
                0x3: "parseVarBind: Expected OID at VarBind index",
                0x4: "parseSequence: Expected SEQUENCE at VarBind index",
                0x5: "parseSequenceOf: Expected SEQUENCE OF at VarBindList",
                0x6: "parseSequenceOf: VarBind parse failed",
                0x7: "parseRequest: Invalid PDU type",
                0x8: "parseRequest: Failed to parse VarBindList",
                0x9: "parseCommunity: Invalid community string length",
                0xA: "parseCommunity: Failed to parse SNMP request",
                0xB: "parseCommunity: Unauthorized community string",
                0xC: "parseVersion: Unsupported SNMP version",
                0xD: "parseVersion: Failed to parse community string",
                0xE: "parseSNMPMessage: Invalid SNMP message header",
                0xF: "parseSNMPMessage: Failed to parse SNMP version",
            },
        },
        0xF: {  # nettask.c
            0x2: {  # WARNING
                0x1: "w5500_check_version failed",
                0x3: "Waiting for config ready",
                0x5: "No PHY link at startup",
                0x6: "PHY link DOWN detected",
                0x7: "Link did not come up after reset; deferring reinit",
                0x8: "W5500 reinit from cached config failed",
                0x9: "Using fallback network defaults",
            },
            0x4: {  # ERROR
                0x1: "w5500_hw_init failed",
                0x2: "SNMP init failed",
                0x3: "Network configuration failed: storage_get_network failed",
                0x4: "Ethernet HW init failed, entering safe loop",
                0x5: "Failed to create NetTask",
            },
        },
    },
    0x3: {  # METER MODULE
        0x0: {  # hlw8032_driver.c
            0x2: {  # WARNING
                0x4: "Failed to acquire UART mutex",
                0x6: "Async calib insufficient samples",
                0x7: "async V-cal failed: invalid readings",
                0x9: "async V-cal ratio out of range",
                0xA: "async I-cal failed: invalid ratio",
            },
            0x4: {  # ERROR
                0x0: "Switch_SetRelayPortBMasked failed (EN=1)",
                0x1: "Switch_SetRelayPortBMasked failed (EN=0)",
                0x2: "Invalid channel in HLW8032 read",
                0x3: "Failed to create UART mutex",
                0x4: "Async calib invalid channel",
                0x5: "Async calib failed to write EEPROM",
                0x6: "Invalid reference voltage",
                0x7: "Invalid channel for current calibration",
                0x8: "Invalid reference current",
                0x9: "Async calib in invalid mode",
            },
        },
        0xF: {  # metertask.c
            0x2: {  # WARNING
                0x0: "CRITICAL: USB supply low",
                0x1: "Two Point Compute: computed slope out of range",
                0x2: "Two Point Compute: computed V0 out of range",
                0x3: "SetTempCalibration: slope out of range",
                0x4: "SetTempCalibration: V0 out of range",
            },
            0x4: {  # ERROR
                0x0: "Failed to create telemetry queue",
                0x1: "NULL pointer in MeterTask_GetSystemTelemetry",
                0x2: "NULL pointer in Single Point Compute",
                0x3: "CRITICAL: 12V supply low",
                0x4: "CRITICAL: Die temperature high",
                0x5: "CRITICAL: Die temperature low",
                0x6: "NULL pointer in Two Point Compute",
            },
        },
    },
    0x4: {  # STORAGE MODULE
        0x0: {  # calibration.c
            0x4: {  # ERROR
                0x1: "Write Sensor Calibration: Length exceeds max",
                0x2: "Read Sensor Calibration: Length exceeds max",
                0x3: "Write Sensor Calibration: Invalid channel",
                0x4: "Null pointer was passed to write calibration data",
                0x5: "Calibration size exceeds max",
                0x6: "Null pointer was passed to read calibration data",
                0x7: "Read calibration length exceeds max",
                0x8: "Compute Single Point: Null pointer",
                0x9: "Compute Two Point: Null pointer",
                0xA: "Compute Two Point: Identical temperature points",
                0xB: "Computed two point slope out of range",
                0xC: "Computed two point V0 out of range",
                0xD: "Apply To MeterTask: Null pointer",
                0xE: "Apply To MeterTask: Invalid calibration data",
                0xF: "Read Sensor Calibration: Invalid channel",
            },
        },
        0x1: {  # channel_labels.c
            0x4: {  # ERROR
                0x0: "EEPROM_WriteChannelLabel invalid input",
                0x1: "EEPROM_ReadChannelLabel invalid input",
                0x2: "EEPROM_ClearChannelLabel invalid channel",
                0x3: "EEPROM_ClearAllChannelLabels failed on channel",
            },
        },
        0x2: {  # energy_monitor.c
            0x4: {  # ERROR
                0x1: "EEPROM_WriteEnergyMonitoring: Write length exceeds size",
                0x2: "EEPROM_ReadEnergyMonitoring: Read length exceeds size",
                0x3: "EEPROM_ResetEnergyMonitoring: Write error",
            },
        },
        0x4: {  # factory_defaults.c
            0x2: {  # WARNING
                0x1: "Firmware version mismatch",
                0x2: "Network Configuration CRC mismatch",
                0x3: "Sensor Calibration read error",
            },
            0x4: {  # ERROR
                0x1: "Factory defaults write encountered errors",
                0x2: "Factory defaults write failed",
            },
        },
        0x5: {  # cat24c256_driver.c
            0x4: {  # ERROR
                0x0: "WriteByte failed",
                0x1: "ReadByte failed",
                0x2: "WriteBuffer: NULL data pointer",
                0x3: "WriteBuffer failed",
                0x4: "ReadBuffer: NULL buffer pointer",
                0x5: "ReadBuffer failed",
                0x6: "Self-test WriteBuffer failed",
                0x7: "Self-test failed: Data mismatch",
            },
        },
        0x6: {  # network.c
            0x2: {  # WARNING
                0x1: "CRC validation failed for device identity",
                0x2: "Failed to read valid network config from EEPROM",
            },
            0x4: {  # ERROR
                0x1: "EEPROM_WriteSystemInfo: Write length exceeds size",
                0x2: "EEPROM_ReadSystemInfo: Read length exceeds size",
                0x3: "Write length exceeds size",
                0x4: "Read length exceeds size",
                0x5: "CRC mismatch",
                0x6: "Write length exceeds size",
                0x7: "Read length exceeds size",
                0x8: "Network config CRC mismatch",
                0x9: "Device identity write error",
                0xA: "CRC mismatch",
            },
        },
        0x8: {  # user_output.c
            0x2: {  # WARNING
                0x0: "Apply preset failed",
            },
            0x4: {  # ERROR
                0x0: "Data size exceeds EEPROM block",
                0x1: "EEPROM write failed",
                0x2: "EEPROM verify mismatch",
                0x3: "Mutex creation failed",
                0x4: "EEPROM mutex timeout on init",
                0x5: "EEPROM mutex timeout on save",
                0x6: "EEPROM mutex timeout on delete",
                0x7: "EEPROM mutex timeout on set startup",
                0x8: "EEPROM mutex timeout on clear startup",
                0x9: "Relay write length exceeds size",
                0xA: "Relay read length exceeds size",
            },
        },
        0x9: {  # user_prefs.c
            0x2: {  # WARNING
                0x0: "Using default user prefs due to read/CRC failure",
            },
            0x4: {  # ERROR
                0x1: "Write length exceeds size",
                0x2: "Read length exceeds size",
                0x3: "Null pointer for user prefs (write)",
                0x4: "Null pointer for user prefs (read)",
                0x5: "User prefs CRC mismatch",
            },
        },
        0xA: {  # device_identity.c
            0x4: {  # ERROR
                0x1: "Device identity: EEPROM write failed",
                0x2: "Device identity: Init failed - EEPROM mutex timeout",
            },
        },
    },
    0x5: {  # BUTTON MODULE
        0x0: {  # button_driver.c
            0x4: {  # ERROR
                0x0: "ButtonDrv_SelectShow: bad index",
                0x1: "ButtonDrv_SelectShow: SwitchTask not ready",
                0x2: "ButtonDrv_SelectShow: enqueue failed",
                0x3: "ButtonDrv_SelectLeft: NULL io_index pointer",
                0x4: "ButtonDrv_SelectRight: NULL io_index pointer",
            },
        },
        0x1: {  # mcp23017_driver.c
            0x2: {  # WARNING
                0x0: "MCP23017 recover failed",
            },
            0x4: {  # ERROR
                0x0: "Invalid register params",
                0x1: "Device registry full",
                0x2: "Mutex create failed",
                0x3: "NULL device in init",
            },
        },
        0xF: {  # buttontask.c
            0x4: {  # ERROR
                0x0: "Button event queue not initialized",
                0x1: "Storage not ready, start timeout",
                0x2: "Button event queue create failed",
                0x3: "Blink timer create failed",
                0x4: "ButtonTask create failed",
            },
        },
    },
    0x6: {  # HEALTH MODULE
        0x1: {  # power_mgr.c
            0x2: {  # WARNING
                0x0: "Already in STANDBY",
                0x1: "Already in RUN mode",
            },
        },
        0x2: {  # rtos_hooks.c
            0xF: {  # FATAL
                0x0: "Stack overflow in task",
            },
        },
        0x3: {  # wrap_watchdog.c
            0xF: {  # FATAL
                0x0: "log_err buffer truncated",
                0x1: "Panic msg",
                0x2: "Hard assert failure",
                0x3: "Invalid params",
                0x4: "reset_usb_boot called",
                0x5: "runtime_unreset_core called",
                0x6: "runtime_reboot called",
                0x7: "__NVIC_SystemReset called",
                0x8: "System reset triggered",
            },
        },
        0xF: {  # healthtask.c
            0x1: {  # INFO
                0x0: "Reboot offenders",
                0x1: "GRACE PERIOD ENDED",
                0x2: "INTENTIONAL REBOOT",
            },
            0x2: {  # WARNING
                0x0: "Scheduler idle stalled",
                0x1: "Waiting first beats",
                0x2: "Task never heartbeated",
                0x3: "Task misbehaving high dt",
                0x4: "Task re-register",
            },
            0xF: {  # FATAL
                0x0: "WDT pre-bark warning",
                0x1: "Reboot pending stale tasks",
                0x2: "WDT pre-bark in task loop",
                0x3: "Last reboot by health task",
                0x4: "Stale tasks detail",
            },
        },
    },
    0x7: {  # LOGGER MODULE
        0x0: {  # helpers.c
            0x4: {  # ERROR
                0x0: "Key not found in form data",
                0x1: "Null networkInfo pointer",
                0x2: "Invalid MAC address format",
            },
        },
        0xF: {  # loggertask.c
            0x4: {  # ERROR
                0x0: "Failed to create logger queue",
            },
        },
    },
    0x8: {  # CONSOLE MODULE
        0x0: {  # consoletask.c (FID 0x0)
            0x4: {  # ERROR
                0x1: "Invalid IP address octet value",
                0x2: "Invalid channel for READ_HLW8032",
                0x3: "EEPROM busy",
                0x4: "EEPROM write failed during CALIB_TEMP 1P",
                0x5: "EEPROM write failed during CALIB_TEMP 2P",
                0x6: "Invalid SET_CH arguments",
                0x7: "Invalid value for SET_CH",
                0x8: "Failed to set CH during SET_CH ALL",
                0x9: "Invalid channel for SET_CH",
                0xA: "Failed to set CH during SET_CH",
                0xB: "Failed to set all channels OFF",
                0xC: "Failed to set all channels ON",
            },
        },
        0x1: {  # consoletask.c (FID 0x1)
            0x4: {  # ERROR
                0x0: "Invalid channel for AUTO_CAL_V",
                0x1: "Invalid reference voltage for AUTO_CAL_V",
                0x2: "AUTO_CAL_V invalid arguments",
                0x3: "Invalid reference current for AUTO_CAL_I",
                0x4: "Invalid channel for AUTO_CAL_I",
                0x5: "AUTO_CAL_I invalid arguments",
                0x6: "Invalid channel for SHOW_CALIB",
                0x7: "Invalid IP address format",
                0x8: "Failed to read network config",
                0x9: "Failed to save IP address",
                0xA: "Invalid subnet mask format",
                0xB: "Failed to read network config",
                0xC: "Failed to save subnet mask",
                0xD: "Invalid gateway format",
                0xE: "Failed to read network config",
                0xF: "Failed to save gateway",
            },
        },
        0x2: {  # consoletask.c (FID 0x2)
            0x4: {  # ERROR
                0x0: "Invalid DNS format",
                0x1: "Failed to read network config",
                0x2: "Failed to save DNS",
                0x3: "Invalid network config arguments",
                0x4: "Failed to read network config",
                0x5: "Invalid IP",
                0x6: "Invalid subnet",
                0x7: "Invalid gateway",
                0x8: "Invalid DNS",
                0x9: "Failed to save network config",
                0xA: "Failed to enqueue error log dump (storage busy)",
                0xB: "Failed to enqueue warning log dump (storage busy)",
                0xC: "Failed to enqueue error log clear (storage busy)",
                0xD: "Failed to enqueue warning log clear (storage busy)",
                0xE: "Queue creation failed",
                0xF: "Failed to create ConsoleTask",
            },
        },
    },
    0x9: {  # OCP MODULE
        0xF: {  # ocp.c
            0x4: {  # ERROR
                0xA: "WARNING: Total current approaching limit",
            },
            0xF: {  # FATAL
                0xB: "CRITICAL: Overcurrent trip executed; switching locked",
            },
        },
    },
    0xA: {  # SWITCH MODULE
        # Note: SwitchTask does not appear to use error codes in the current implementation
        # Reserved for future use
    },
}
