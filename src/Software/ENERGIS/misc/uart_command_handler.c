/**
 * @file uart_command_handler.c
 * @author David Sipos (DvidMakesThings)
 * @defgroup uart UART Command Handler
 * @brief UART command handler for ENERGIS PDU (10-Inch Rack).
 * @{
 * @version 1.0
 * @date 2025-03-03
 *
 * Implements command parsing and execution over UART (USB-CDC).
 * Provides network, system, and EEPROM control via textual commands.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "CONFIG.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern volatile uint32_t bootloader_trigger;
extern wiz_NetInfo g_net_info; // current network info in RAM

/**
 * @brief Remove leading/trailing whitespace and non-printable characters from a string.
 *
 * Cleans the input string in-place, removing unwanted characters and whitespace.
 *
 * @param s Pointer to the string to be trimmed.
 * @return Pointer to the cleaned string.
 */
static char *trim(char *s) {
    // Skip leading whitespace
    while (*s && isspace((unsigned char)*s))
        s++;

    // Early out if string is empty after trim
    if (*s == '\0')
        return s;

    // Remove trailing whitespace + non-printables
    char *end = s + strlen(s) - 1;
    while (end > s && (isspace((unsigned char)*end) || !isprint((unsigned char)*end))) {
        *end-- = '\0';
    }

    // Clean inline non-printables / control characters
    char *read = s;
    char *write = s;
    while (*read) {
        unsigned char c = *read++;
        if (isprint(c)) {
            *write++ = c;
        }
        // else: skip glitch byte silently
    }
    *write = '\0';

    return s;
}

/**
 * @brief Parse and execute a received UART command string.
 *
 * Handles all supported commands, dispatching to the appropriate handler.
 *
 * @param cmd_in The input command string.
 */
static void process_command(const char *cmd_in) {
    char cmd[128];
    strncpy(cmd, cmd_in, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';

    // Trim and uppercase
    char *trimmed = trim(cmd);
    for (char *p = trimmed; *p; ++p)
        *p = toupper(*p);

    ECHO("Received CMD: \"%s\"\r\n", cmd);
    if (strcmp(trimmed, "HELP") == 0) {
        printHelp();
    } else if (strcmp(trimmed, "BOOTSEL") == 0) {
        bootsel_trigger();
    } else if (strcmp(trimmed, "REBOOT") == 0) {
        reboot();
    } else if (strcmp(trimmed, "DUMP_EEPROM") == 0) {
        dump_eeprom();
    } else if (strcmp(trimmed, "RFS") == 0) {
        EEPROM_WriteFactoryDefaults();
        watchdog_reboot(0, 0, 0);
    } else if (strncmp(trimmed, "SET_IP ", 7) == 0) {
        set_ip(trimmed + 7, trimmed);
    } else if (strncmp(trimmed, "SET_DNS ", 7) == 0) {
        set_dns(trimmed + 7, trimmed);
    } else if (strncmp(trimmed, "SET_GW ", 7) == 0) {
        set_gateway(trimmed + 7, trimmed);
    } else if (strncmp(trimmed, "SET_SN ", 7) == 0) {
        set_subnet(trimmed + 7, trimmed);
    } else if (strncmp(trimmed, "CONFIG_NETWORK ", 15) == 0) {
        set_network(trimmed + 15, trimmed);
    } else if (strcmp(trimmed, "NETINFO") == 0) {
        ECHO("NETWORK INFORMATION:\r\n");
        print_network_information(g_net_info);
    } else if (strcmp(trimmed, "SYSINFO") == 0) {
        ECHO("SYSTEM INFORMATION:\r\n");
        getSysInfo();
    } else if (strcmp(trimmed, "BAADCAFE") == 0) {
        EEPROM_EraseAll();
    } else if (strcmp(trimmed, "READ_HLW8032") == 0) {
        hlw8032_readings();
    } else if (strncmp(trimmed, "READ_HLW8032 ", 13) == 0) {
        int ch = atoi(trimmed + 13);
        if (ch >= 1 && ch <= 8) {
            hlw8032_read_channel((uint8_t)ch);
        } else {
            setError(false);
            ERROR_PRINT("Invalid channel: %d\r\n", ch);
        }
    } else if (strcmp(trimmed, "GET_TEMP") == 0) {
        read_dieTemp();
    } else if (strncmp(trimmed, "SET_CH ", 7) == 0) {
        handle_uart_set_ch_command(trimmed);
    } else if (strncmp(trimmed, "GET_CH ", 7) == 0) {
        handle_uart_get_ch_command(trimmed);
    } else if (strcmp(trimmed, "CLR_ERR") == 0) {
        setError(false);
    } else if (strcmp(trimmed, "GET_SUPPLY") == 0) {
        float voltage = get_Voltage(V_SUPPLY) * (110000.0f / 10000.0f);
        ECHO("12V SUPPLY VOLTAGE: %.3f V\r\n", voltage);
    } else if (strcmp(trimmed, "GET_USB") == 0) {
        float voltage = get_Voltage(V_USB) * (20000.0f / 10000.0f);
        ECHO("USB SUPPLY VOLTAGE: %.3f V\r\n", voltage);
    } else {
        setError(true);
        ERROR_PRINT("Unknown command \"%s\"\r\n", trimmed);
        printHelp();
    }
}

/**
 * @brief Main loop for reading and processing UART commands.
 *
 * Reads characters from UART, assembles lines, and executes commands.
 */
void uart_command_loop(void) {
    static char line[UART_CMD_BUF_LEN];
    static size_t idx = 0;

    int c;
    // Read all available chars without blocking
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (idx) {
                line[idx] = '\0';
                char *cmd = trim(line);
                if (*cmd)
                    process_command(cmd);
                idx = 0;
            }
        } else {
            // store printable non-null chars
            if (idx < sizeof(line) - 1 && c >= 32 && c < 127) {
                line[idx++] = (char)c;
            }
        }
    }
}

/**
 * @brief Set flag to enter BOOTSEL mode on next reboot.
 *
 * Triggers the bootloader for firmware update via BOOTSEL.
 */
void bootsel_trigger(void) {
    // trigger BOOTSEL on next reboot
    bootloader_trigger = 0xDEADBEEF;
    ECHO("OK: ENTERING BOOTSEL\r\n");
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Immediately reboot the system.
 *
 * Performs a software reset using the watchdog.
 */
void reboot(void) {
    ECHO("OK: REBOOTING\r\n");
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Print help information for available commands.
 * @return None
 */
void printHelp() {
    printf("\nAvailable commands:\r\n");
    printf("%-32s %s\r\n", "\tHELP", "Show this help message");
    printf("%-32s %s\r\n", "\tSYSINFO", "Show system information");
    printf("%-32s %s\r\n", "\tREBOOT", "Reboot the device");
    printf("%-32s %s\r\n", "\tBOOTSEL", "Reset the device into bootloader mode");
    printf("%-32s %s\r\n", "\t------------ Network Settings ------------", "");
    printf("%-32s %s\r\n", "\tCONFIG_NETWORK <ip$sn$gw$dns>", "Configure network settings");
    printf("%-32s %s\r\n", "\tSET_IP <xxx.xxx.xxx.xxx>", "Set the device IP address");
    printf("%-32s %s\r\n", "\tSET_DNS <x.x.x.x>", "Set the DNS server address");
    printf("%-32s %s\r\n", "\tSET_GW <xxx.xxx.xxx.xxx>", "Set the gateway address");
    printf("%-32s %s\r\n", "\tSET_SN <xxx.xxx.xxx.xxx>", "Set the subnet mask");
    printf("%-32s %s\r\n", "\tNETINFO", "Show network information");
    printf("%-32s %s\r\n", "\t------------ Output Functions ------------", "");
    printf("%-32s %s\r\n", "\tSET_CH <n|ALL> <ON|OFF>", "Set channel relay state");
    printf("%-32s %s\r\n", "\tGET_CH <n|ALL>", "Get channel relay state");
    printf("%-32s %s\r\n", "\tREAD_HLW8032", "Read HLW8032 sensor data");
    printf("%-32s %s\r\n", "\tREAD_HLW8032 <ch>", "Read specific HLW8032 channel");
    printf("%-32s %s\r\n", "\t------------ Other Functions ------------", "");
    printf("%-32s %s\r\n", "\tDUMP_EEPROM", "Create snapshot from the EEPROM");
    printf("%-32s %s\r\n", "\tCLR_ERR", "Clear errors");
    printf("%-32s %s\r\n", "\tGET_TEMP", "Get die temperature");
    printf("%-32s %s\r\n", "\tRFS", "Reset to factory settings");
    printf("%-32s %s\r\n", "\tBAADCAFE", "Erase entire EEPROM (all settings lost)");
    printf("%-32s %s\r\n", "\tGET_SUPPLY", "Get voltage from 12V supply sense line");
    printf("%-32s %s\r\n", "\tGET_USB", "Get voltage from USB supply sense line");
}

/**
 * @brief Handle UART SET_CH command.
 *        Uses set_relay_state_tag("UART", ...) to log who wrote.
 * @param trimmed The trimmed command string.
 * @return void
 */
void handle_uart_set_ch_command(const char *trimmed) {
    // Format: SET_CH <n|ALL> <ON|OFF>
    char arg[16] = {0}, state[8] = {0};
    if (sscanf(trimmed + 7, "%15s %7s", arg, state) != 2) {
        setError(true);
        ERROR_PRINT("Usage: SET_CH <n|ALL> <ON|OFF>\r\n");
        return;
    }

    uint8_t want_on = (strcmp(state, "ON") == 0) ? 1u : 0u;

    if (strcmp(arg, "ALL") == 0) {
        int changed_total = 0;
        uint8_t any_asym_before = 0, any_asym_after = 0;

        for (uint8_t i = 0; i < 8; i++) {
            uint8_t ab = 0, aa = 0;
            changed_total += set_relay_state_with_tag(UART_CMH_TAG, i, want_on, &ab, &aa);
            any_asym_before |= ab;
            any_asym_after |= aa;
        }

        ECHO("OK: All channels set %s (changed=%d)\r\n", want_on ? "ON" : "OFF", changed_total);

        if (any_asym_before || any_asym_after) {
            uint16_t mask = mcp_dual_asymmetry_mask();
            WARNING_PRINT("Dual asymmetry %s (mask=0x%04X)\r\n",
                          any_asym_after ? "PERSISTING" : "DETECTED", (unsigned)mask);
        }
        return;
    }

    int n = atoi(arg);
    if (n < 1 || n > 8) {
        setError(true);
        ERROR_PRINT("Invalid channel: %s\r\n", arg);
        return;
    }

    uint8_t ab = 0, aa = 0;
    uint8_t changed = set_relay_state_with_tag(UART_CMH_TAG, (uint8_t)(n - 1), want_on, &ab, &aa);

    ECHO("OK: Channel %d set %s%s\r\n", n, want_on ? "ON" : "OFF", changed ? "" : " (no change)");

    if (ab || aa) {
        uint16_t mask = mcp_dual_asymmetry_mask();
        WARNING_PRINT("Dual asymmetry %s (mask=0x%04X)\r\n", aa ? "PERSISTING" : "DETECTED",
                      (unsigned)mask);
    }
}

/**
 * @brief Handle UART GET_CH command.
 * @param trimmed The trimmed command string.
 * Parses and executes the GET_CH command to read relay channel states.
 * Supported formats:
 *    - GET_CH <n>     (1..8)
 *    - GET_CH ALL     (prints all channels 1..8)
 * @return None
 */
void handle_uart_get_ch_command(const char *trimmed) {
    // Expect: "GET_CH " (7 chars), then argument
    const char *args = trimmed + 7;
    while (*args == ' ')
        args++;

    char arg[16] = {0};
    if (sscanf(args, "%15s", arg) != 1) {
        setError(true);
        ERROR_PRINT("Usage: GET_CH <n|ALL>\r\n");
        return;
    }

    if (strcmp(arg, "ALL") == 0) {
        // Print all channel states 1..8
        for (int ch = 1; ch <= 8; ch++) {
            uint8_t state = mcp_relay_read_pin((uint8_t)(ch - 1));
            ECHO("CH%d: %s\r\n", ch, state ? "ON" : "OFF");
        }
        return;
    }

    // Single channel path
    int ch = atoi(arg);
    if (ch >= 1 && ch <= 8) {
        uint8_t state = mcp_relay_read_pin((uint8_t)(ch - 1));
        ECHO("CH%d: %s\r\n", ch, state ? "ON" : "OFF");
    } else {
        setError(true);
        ERROR_PRINT("Invalid channel: %s\r\n", arg);
    }
}

/**
 * @brief Print system information (core voltage, clocks, serial, firmware).
 *
 * Reads device serial number and firmware version from EEPROM (SYS_INFO area),
 * with graceful fallbacks if data is missing or checksum fails. Then prints
 * voltage selector, core voltage estimate, and key clock frequencies.
 *
 * @par EEPROM SYS_INFO layout (variable-length, null-terminated strings):
 *   [0 .. N]   : Device Serial (ASCII, '\0' terminated)
 *   [N+1 .. M] : Firmware Version (ASCII, '\0' terminated)  <-- optional
 *
 * @par Fallbacks:
 *   - If EEPROM is empty/invalid: serial <- DEFAULT_SN, fw <- FW_VERSION
 *   - If only serial exists: fw <- FW_VERSION
 *
 * @note Uses ECHO(...) for UART-friendly, parseable output lines.
 *       Keeps previous OK header and format style.
 */
void getSysInfo(void) {
    /* -------- Read HW status (voltage/clocks) -------- */
    uint sys_freq = clock_get_hz(clk_sys);
    uint usb_freq = clock_get_hz(clk_usb);
    uint peri_freq = clock_get_hz(clk_peri);
    uint adc_freq = clock_get_hz(clk_adc);

    uint32_t vreg_raw = *((volatile uint32_t *)VREG_BASE);
    uint vsel = vreg_raw & VREG_VSEL_MASK;
    float voltage = 1.10f + 0.05f * vsel;

    /* -------- Read serial & firmware from EEPROM -------- */
    char serial_buf[64] = {0};
    char fw_buf[32] = {0};
    int have_serial = 0, have_fw = 0;

    /* We read the whole SYS_INFO block into a temp buffer, then split on NULs */
    uint8_t sys_area[EEPROM_SYS_INFO_SIZE] = {0};
    EEPROM_ReadSystemInfo(sys_area, EEPROM_SYS_INFO_SIZE);

    if (sys_area[0] != 0) {
        size_t s_len = strnlen((const char *)sys_area, EEPROM_SYS_INFO_SIZE);
        if (s_len > 0 && s_len < sizeof(serial_buf)) {
            memcpy(serial_buf, sys_area, s_len);
            serial_buf[s_len] = '\0';
            have_serial = 1;

            size_t pos = s_len + 1;
            if (pos < EEPROM_SYS_INFO_SIZE && sys_area[pos] != 0) {
                size_t f_len = strnlen((const char *)&sys_area[pos], EEPROM_SYS_INFO_SIZE - pos);
                if (f_len > 0 && f_len < sizeof(fw_buf)) {
                    memcpy(fw_buf, &sys_area[pos], f_len);
                    fw_buf[f_len] = '\0';
                    have_fw = 1;
                }
            }
        }
    }

    /* -------- Print in a stable, parseable format -------- */
    ECHO("Device Serial : %s\r\n", serial_buf);
    ECHO("Firmware Ver  : %s\r\n", fw_buf);
    ECHO("Core voltage  : %.2f V (vsel = %u)\r\n", voltage, vsel);
    ECHO("Clock Sources :\r\n");
    ECHO("\tSYS: %u Hz (%u MHz)\r\n", sys_freq, sys_freq / 1000000);
    ECHO("\tUSB: %u Hz (%u MHz)\r\n", usb_freq, usb_freq / 1000000);
    ECHO("\tPER: %u Hz (%u MHz)\r\n", peri_freq, peri_freq / 1000000);
    ECHO("\tADC: %u Hz (%u MHz)\r\n", adc_freq, adc_freq / 1000000);
}

/**
 * @brief Dump the contents of EEPROM in formatted hex.
 *
 * Reads and prints all EEPROM data for inspection.
 */
void dump_eeprom(void) {
    // dump EEPROM, then acknowledge
    CAT24C512_DumpFormatted();
    ECHO("OK: DUMP_EEPROM\r\n");
}

/**
 * @brief Set the device IP address and reconfigure network.
 *
 * Updates the IP address, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param ip The new IP address (dotted-decimal string).
 * @param cmd The full command string.
 */
void set_ip(const char *ip, const char *cmd) {
    unsigned ip0, ip1, ip2, ip3;
    char ip_str[16];
    strncpy(ip_str, cmd + 7, sizeof(ip_str));
    ip_str[sizeof(ip_str) - 1] = '\0';

    if (sscanf(ip_str, "%u.%u.%u.%u", &ip0, &ip1, &ip2, &ip3) != 4 || ip0 > 255 || ip1 > 255 ||
        ip2 > 255 || ip3 > 255) {
        setError(true);
        ERROR_PRINT("Invalid IP \"%s\"\r\n", ip_str);
        return;
    }

    // 1) Load existing settings (with CRC check)
    networkInfo net = LoadUserNetworkConfig();

    // 2) Replace only the IP bytes
    net.ip[0] = (uint8_t)ip0;
    net.ip[1] = (uint8_t)ip1;
    net.ip[2] = (uint8_t)ip2;
    net.ip[3] = (uint8_t)ip3;

    // 3) Write back to EEPROM
    if (EEPROM_WriteUserNetworkWithChecksum(&net) != 0) {
        setError(true);
        ERROR_PRINT("Failed to save IP\r\n");
        return;
    }

    // 4) (Re)configure the Wiznet chip immediately
    wiz_NetInfo wiz = {
        .mac = {net.mac[0], net.mac[1], net.mac[2], net.mac[3], net.mac[4], net.mac[5]},
        .ip = {net.ip[0], net.ip[1], net.ip[2], net.ip[3]},
        .sn = {net.sn[0], net.sn[1], net.sn[2], net.sn[3]},
        .gw = {net.gw[0], net.gw[1], net.gw[2], net.gw[3]},
        .dns = {net.dns[0], net.dns[1], net.dns[2], net.dns[3]},
        .dhcp = net.dhcp};
    wizchip_setnetinfo(&wiz);

    ECHO("OK: SET_IP %s\r\nRebooting...\r\n", ip_str);
    watchdog_reboot(0, 0, 0);
    return;
}

/**
 * @brief Set the subnet mask and reconfigure network.
 *
 * Updates the subnet mask, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param mask The new subnet mask (dotted-decimal string).
 * @param cmd The full command string.
 */
void set_subnet(const char *mask, const char *cmd) {
    unsigned sn0, sn1, sn2, sn3;
    char sn_str[16];
    strncpy(sn_str, cmd + 7, sizeof(sn_str));
    sn_str[sizeof(sn_str) - 1] = '\0';

    if (sscanf(sn_str, "%u.%u.%u.%u", &sn0, &sn1, &sn2, &sn3) != 4 || sn0 > 255 || sn1 > 255 ||
        sn2 > 255 || sn3 > 255) {
        setError(true);
        ERROR_PRINT("Invalid Subnet Mask \"%s\"\r\n", sn_str);
        return;
    }

    networkInfo net = LoadUserNetworkConfig();
    net.sn[0] = (uint8_t)sn0;
    net.sn[1] = (uint8_t)sn1;
    net.sn[2] = (uint8_t)sn2;
    net.sn[3] = (uint8_t)sn3;

    if (EEPROM_WriteUserNetworkWithChecksum(&net) != 0) {
        setError(true);
        ERROR_PRINT("Failed to save Subnet Mask\r\n");
        return;
    }

    wiz_NetInfo wiz = {
        .mac = {net.mac[0], net.mac[1], net.mac[2], net.mac[3], net.mac[4], net.mac[5]},
        .ip = {net.ip[0], net.ip[1], net.ip[2], net.ip[3]},
        .sn = {net.sn[0], net.sn[1], net.sn[2], net.sn[3]},
        .gw = {net.gw[0], net.gw[1], net.gw[2], net.gw[3]},
        .dns = {net.dns[0], net.dns[1], net.dns[2], net.dns[3]},
        .dhcp = net.dhcp};
    wizchip_setnetinfo(&wiz);

    ECHO("OK: SET_SUBNET %s\r\nRebooting...\r\n", sn_str);
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Set the gateway address and reconfigure network.
 *
 * Updates the gateway, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param gw The new gateway address (dotted-decimal string).
 * @param cmd The full command string.
 */
void set_gateway(const char *gw, const char *cmd) {
    unsigned gw0, gw1, gw2, gw3;
    char gw_str[16];
    strncpy(gw_str, cmd + 7, sizeof(gw_str));
    gw_str[sizeof(gw_str) - 1] = '\0';

    if (sscanf(gw_str, "%u.%u.%u.%u", &gw0, &gw1, &gw2, &gw3) != 4 || gw0 > 255 || gw1 > 255 ||
        gw2 > 255 || gw3 > 255) {
        setError(true);
        ERROR_PRINT("Invalid Gateway \"%s\"\r\n", gw_str);
        return;
    }

    networkInfo net = LoadUserNetworkConfig();
    net.gw[0] = (uint8_t)gw0;
    net.gw[1] = (uint8_t)gw1;
    net.gw[2] = (uint8_t)gw2;
    net.gw[3] = (uint8_t)gw3;

    if (EEPROM_WriteUserNetworkWithChecksum(&net) != 0) {
        setError(true);
        ERROR_PRINT("Failed to save Gateway\r\n");
        return;
    }

    wiz_NetInfo wiz = {
        .mac = {net.mac[0], net.mac[1], net.mac[2], net.mac[3], net.mac[4], net.mac[5]},
        .ip = {net.ip[0], net.ip[1], net.ip[2], net.ip[3]},
        .sn = {net.sn[0], net.sn[1], net.sn[2], net.sn[3]},
        .gw = {net.gw[0], net.gw[1], net.gw[2], net.gw[3]},
        .dns = {net.dns[0], net.dns[1], net.dns[2], net.dns[3]},
        .dhcp = net.dhcp};
    wizchip_setnetinfo(&wiz);

    ECHO("OK: SET_GATEWAY %s\r\nRebooting...\r\n", gw_str);
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Set the DNS server address and reconfigure network.
 *
 * Updates the DNS address, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param dns The new DNS address (dotted-decimal string).
 * @param cmd The full command string.
 */
void set_dns(const char *dns, const char *cmd) {
    unsigned d0, d1, d2, d3;
    char dns_str[16];
    strncpy(dns_str, cmd + 8, sizeof(dns_str));
    dns_str[sizeof(dns_str) - 1] = '\0';

    if (sscanf(dns_str, "%u.%u.%u.%u", &d0, &d1, &d2, &d3) != 4 || d0 > 255 || d1 > 255 ||
        d2 > 255 || d3 > 255) {
        setError(true);
        ERROR_PRINT("Invalid DNS \"%s\"\r\n", dns_str);
        return;
    }

    networkInfo net = LoadUserNetworkConfig();
    net.dns[0] = (uint8_t)d0;
    net.dns[1] = (uint8_t)d1;
    net.dns[2] = (uint8_t)d2;
    net.dns[3] = (uint8_t)d3;

    if (EEPROM_WriteUserNetworkWithChecksum(&net) != 0) {
        setError(true);
        ERROR_PRINT("Failed to save DNS\r\n");
        return;
    }

    wiz_NetInfo wiz = {
        .mac = {net.mac[0], net.mac[1], net.mac[2], net.mac[3], net.mac[4], net.mac[5]},
        .ip = {net.ip[0], net.ip[1], net.ip[2], net.ip[3]},
        .sn = {net.sn[0], net.sn[1], net.sn[2], net.sn[3]},
        .gw = {net.gw[0], net.gw[1], net.gw[2], net.gw[3]},
        .dns = {net.dns[0], net.dns[1], net.dns[2], net.dns[3]},
        .dhcp = net.dhcp};
    wizchip_setnetinfo(&wiz);

    ECHO("OK: SET_DNS %s\r\nRebooting...\r\n", dns_str);
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Set full network configuration (IP, subnet, gateway, DNS).
 *
 * Updates all network parameters, saves to EEPROM, and applies to Wiznet chip.
 *
 * @param full_cmd The full command string with parameters.
 * @param cmd The full command string.
 */
void set_network(const char *full_cmd, const char *cmd) {
    const char *params = cmd + 15; // Skip "CONFIG_NETWORK "
    char buffer[128];
    strncpy(buffer, params, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char *ip_str = strtok(buffer, "$");
    char *sn_str = strtok(NULL, "$");
    char *gw_str = strtok(NULL, "$");
    char *dns_str = strtok(NULL, "$");

    if (!ip_str || !sn_str || !gw_str || !dns_str) {
        setError(true);
        ERROR_PRINT("Missing parameters in CONFIG_NETWORK\r\n");
        return;
    }

    unsigned ip0, ip1, ip2, ip3;
    unsigned sn0, sn1, sn2, sn3;
    unsigned gw0, gw1, gw2, gw3;
    unsigned d0, d1, d2, d3;

    if (sscanf(ip_str, "%u.%u.%u.%u", &ip0, &ip1, &ip2, &ip3) != 4 ||
        sscanf(sn_str, "%u.%u.%u.%u", &sn0, &sn1, &sn2, &sn3) != 4 ||
        sscanf(gw_str, "%u.%u.%u.%u", &gw0, &gw1, &gw2, &gw3) != 4 ||
        sscanf(dns_str, "%u.%u.%u.%u", &d0, &d1, &d2, &d3) != 4 || ip0 > 255 || ip1 > 255 ||
        ip2 > 255 || ip3 > 255 || sn0 > 255 || sn1 > 255 || sn2 > 255 || sn3 > 255 || gw0 > 255 ||
        gw1 > 255 || gw2 > 255 || gw3 > 255 || d0 > 255 || d1 > 255 || d2 > 255 || d3 > 255) {
        setError(true);
        ERROR_PRINT("Invalid IP format in CONFIG_NETWORK\r\n");
        return;
    }

    networkInfo net = LoadUserNetworkConfig();
    net.ip[0] = ip0;
    net.ip[1] = ip1;
    net.ip[2] = ip2;
    net.ip[3] = ip3;
    net.sn[0] = sn0;
    net.sn[1] = sn1;
    net.sn[2] = sn2;
    net.sn[3] = sn3;
    net.gw[0] = gw0;
    net.gw[1] = gw1;
    net.gw[2] = gw2;
    net.gw[3] = gw3;
    net.dns[0] = d0;
    net.dns[1] = d1;
    net.dns[2] = d2;
    net.dns[3] = d3;

    if (EEPROM_WriteUserNetworkWithChecksum(&net) != 0) {
        setError(true);
        ("Failed to save network config\r\n");
        return;
    }

    wiz_NetInfo wiz = {
        .mac = {net.mac[0], net.mac[1], net.mac[2], net.mac[3], net.mac[4], net.mac[5]},
        .ip = {net.ip[0], net.ip[1], net.ip[2], net.ip[3]},
        .sn = {net.sn[0], net.sn[1], net.sn[2], net.sn[3]},
        .gw = {net.gw[0], net.gw[1], net.gw[2], net.gw[3]},
        .dns = {net.dns[0], net.dns[1], net.dns[2], net.dns[3]},
        .dhcp = net.dhcp};
    wizchip_setnetinfo(&wiz);

    ECHO("OK: CONFIG_NETWORK %s$%s$%s$%s\r\nRebooting...\r\n", ip_str, sn_str, gw_str, dns_str);
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Read and print HLW8032 sensor values for all channels.
 *
 * Iterates through all 8 HLW8032 channels and prints voltage, current, and power.
 */
void hlw8032_readings(void) {
    ECHO("Reading HLW8032 values for all 8 channels...\n");
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (hlw8032_read(ch)) {
            float voltage = hlw8032_get_voltage();
            float current = hlw8032_get_current();
            float power = hlw8032_get_power();
            ECHO("CH%u: V=%.2f V, I=%.3f A, P=%.2f W\n\n", ch + 1, voltage, current, power);
        } else {
            setError(true);
            ERROR_PRINT("CH%u: Failed to read HLW8032 data\n", ch + 1);
        }
    }
}

/**
 * @brief Read and print HLW8032 sensor values for a specific channel.
 *
 * Reads voltage, current, and power for the given HLW8032 channel.
 *
 * @param channel Channel index (1–8) to read from.
 */
void hlw8032_read_channel(uint8_t channel) {
    ECHO("Reading HLW8032 values for channel %u...\n", channel);
    if (hlw8032_read(channel)) {
        float voltage = hlw8032_get_voltage();
        float current = hlw8032_get_current();
        float power = hlw8032_get_power();
        ECHO("CH%u: V=%.2f V, I=%.3f A, P=%.2f W\n", channel, voltage, current, power);
    } else {
        setError(true);
        ERROR_PRINT("CH%u: Failed to read HLW8032 data\n", channel);
    }
}

/**
 * @brief Read and print the die temperature.
 *
 * Reads the die temperature from the ADC and prints it.
 * Also converts the raw ADC value to a temperature in Celsius.
 *
 * @return None
 */
void read_dieTemp(void) {
    ECHO("OK: Reading die temperature...\n");
    adc_select_input(4);
    uint16_t raw = adc_read();
    float v_die = raw * 3.0f / 4096.0f;
    float temp_c = 27.0f - (v_die - 0.706f) / 0.001721f;
    PLOT("temp_sensor_voltage: %.5f\n", v_die);
    PLOT("temp_sensor_temperature: %.3f\n", temp_c);
}

/**
 * @}
 */