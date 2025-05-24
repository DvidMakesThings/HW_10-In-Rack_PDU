/**
 * @file uart_command_handler.c
 * @author DvidMakesThings - David Sipos
 * @brief UART command handler over stdio (USB-CDC) for ENERGIS PDU.
 * @version 1.0
 * @date 2025-03-03
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
 * @brief Trims leading/trailing whitespace and removes non-printable or glitch bytes from a string.
 * @param s The string to clean (in-place).
 * @return Cleaned string pointer.
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
 * @brief Process the command received from UART.
 * @param cmd The command string to process.
 */
static void process_command(const char *cmd_in) {
    char cmd[128];
    strncpy(cmd, cmd_in, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';

    // Trim and uppercase
    char *trimmed = trim(cmd);
    for (char *p = trimmed; *p; ++p)
        *p = toupper(*p);

    INFO_PRINT("Received CMD: \"%s\"\r\n", cmd);
    if (strcmp(trimmed, "HELP") == 0) {
        printf("\nAvailable commands:\r\n"
               "HELP\r\n"
               "BOOTSEL\r\n"
               "DUMP_EEPROM\r\n"
               "REBOOT\r\n"
               "SET_IP <addr>\r\n"
               "SET_DNS <addr>\r\n"
               "SET_GW <addr>\r\n"
               "SET_SN <addr>\r\n"
               "NETINFO\r\n"
               "SYSINFO\r\n");
    } else if (strcmp(trimmed, "BOOTSEL") == 0) {
        bootsel_trigger();
    } else if (strcmp(trimmed, "REBOOT") == 0) {
        reboot();
    } else if (strcmp(trimmed, "DUMP_EEPROM") == 0) {
        dump_eeprom();
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
        INFO_PRINT("NETWORK INFORMATION:\r\n");
        print_network_information(g_net_info);
    } else if (strcmp(trimmed, "SYSINFO") == 0) {
        INFO_PRINT("SYSTEM INFORMATION:\r\n");
        getSysInfo();
    } else if (strcmp(trimmed, "READ_HLW8032") == 0) {
        test_hlw8032_readings();
    } else if (strncmp(trimmed, "READ_HLW8032 ", 13) == 0) {
        uint8_t ch = (uint8_t)atoi(trimmed + 13);
        test_hlw8032_read_channel(ch);
    } else {
        ERROR_PRINT("Unknown command \"%s\"\r\n", trimmed);
    }
}

/**
 * @brief Main command loop for UART input.
 *
 * Continuously reads from the UART and processes commands.
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
 * @brief Trigger the BOOTSEL mode on the next reboot.
 *
 * This function sets a flag to enter BOOTSEL mode on the next reboot.
 */
void bootsel_trigger(void) {
    // trigger BOOTSEL on next reboot
    bootloader_trigger = 0xDEADBEEF;
    INFO_PRINT("OK: ENTERING BOOTSEL\r\n");
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Reboot the system.
 *
 * This function reboots the system immediately.
 */
void reboot(void) {
    INFO_PRINT("OK: REBOOTING\r\n");
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Get system information.
 *
 * This function retrieves and prints system information such as clock frequencies and voltage.
 */
void getSysInfo(void) {
    uint sys_freq = clock_get_hz(clk_sys);
    uint usb_freq = clock_get_hz(clk_usb);
    uint peri_freq = clock_get_hz(clk_peri);
    uint adc_freq = clock_get_hz(clk_adc);

    uint32_t vreg_raw = *((volatile uint32_t *)VREG_BASE);
    uint vsel = vreg_raw & VREG_VSEL_MASK;
    float voltage = 1.10f + 0.05f * vsel;

    INFO_PRINT("Core voltage: %.2f V (vsel = %u)\n", voltage, vsel);
    INFO_PRINT("Clock Sources:\n\tSYS: %u Hz\n\tUSB: %u Hz\n\tPER: %u Hz\n\tADC: %u Hz\n", sys_freq,
               usb_freq, peri_freq, adc_freq);
}

/**
 * @brief Dump the EEPROM contents.
 *
 * This function dumps the entire EEPROM contents in a formatted hex table.
 */
void dump_eeprom(void) {
    // dump EEPROM, then acknowledge
    CAT24C512_DumpFormatted();
    INFO_PRINT("OK: DUMP_EEPROM\r\n");
}

/**
 * @brief Set the IP address of the device.
 *
 * This function sets the IP address of the device and reconfigures the Wiznet chip.
 * @param ip The new IP address in dotted-decimal format.
 * @param cmd The command string containing the IP address.
 */
void set_ip(const char *ip, const char *cmd) {
    unsigned ip0, ip1, ip2, ip3;
    char ip_str[16];
    strncpy(ip_str, cmd + 7, sizeof(ip_str));
    ip_str[sizeof(ip_str) - 1] = '\0';

    if (sscanf(ip_str, "%u.%u.%u.%u", &ip0, &ip1, &ip2, &ip3) != 4 || ip0 > 255 || ip1 > 255 ||
        ip2 > 255 || ip3 > 255) {
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

    INFO_PRINT("OK: SET_IP %s\r\nRebooting...\r\n", ip_str);
    watchdog_reboot(0, 0, 0);
    return;
}

/**
 * @brief Set the subnet mask of the device.
 *
 * This function sets the subnet mask of the device and reconfigures the Wiznet chip.
 * @param mask The new subnet mask in dotted-decimal format.
 * @param cmd The command string containing the subnet mask.
 */
void set_subnet(const char *mask, const char *cmd) {
    unsigned sn0, sn1, sn2, sn3;
    char sn_str[16];
    strncpy(sn_str, cmd + 7, sizeof(sn_str));
    sn_str[sizeof(sn_str) - 1] = '\0';

    if (sscanf(sn_str, "%u.%u.%u.%u", &sn0, &sn1, &sn2, &sn3) != 4 || sn0 > 255 || sn1 > 255 ||
        sn2 > 255 || sn3 > 255) {
        ERROR_PRINT("Invalid Subnet Mask \"%s\"\r\n", sn_str);
        return;
    }

    networkInfo net = LoadUserNetworkConfig();
    net.sn[0] = (uint8_t)sn0;
    net.sn[1] = (uint8_t)sn1;
    net.sn[2] = (uint8_t)sn2;
    net.sn[3] = (uint8_t)sn3;

    if (EEPROM_WriteUserNetworkWithChecksum(&net) != 0) {
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

    INFO_PRINT("OK: SET_SUBNET %s\r\nRebooting...\r\n", sn_str);
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Set the gateway address of the device.
 * This function sets the gateway address of the device and reconfigures the Wiznet chip.
 * @param gw The new gateway address in dotted-decimal format.
 * @param cmd The command string containing the gateway address.
 */
void set_gateway(const char *gw, const char *cmd) {
    unsigned gw0, gw1, gw2, gw3;
    char gw_str[16];
    strncpy(gw_str, cmd + 7, sizeof(gw_str));
    gw_str[sizeof(gw_str) - 1] = '\0';

    if (sscanf(gw_str, "%u.%u.%u.%u", &gw0, &gw1, &gw2, &gw3) != 4 || gw0 > 255 || gw1 > 255 ||
        gw2 > 255 || gw3 > 255) {
        ERROR_PRINT("Invalid Gateway \"%s\"\r\n", gw_str);
        return;
    }

    networkInfo net = LoadUserNetworkConfig();
    net.gw[0] = (uint8_t)gw0;
    net.gw[1] = (uint8_t)gw1;
    net.gw[2] = (uint8_t)gw2;
    net.gw[3] = (uint8_t)gw3;

    if (EEPROM_WriteUserNetworkWithChecksum(&net) != 0) {
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

    INFO_PRINT("OK: SET_GATEWAY %s\r\nRebooting...\r\n", gw_str);
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Set the DNS server address of the device.
 * This function sets the DNS server address of the device and reconfigures the Wiznet chip.
 * @param dns The new DNS server address in dotted-decimal format.
 * @param cmd The command string containing the DNS server address.
 */
void set_dns(const char *dns, const char *cmd) {
    unsigned d0, d1, d2, d3;
    char dns_str[16];
    strncpy(dns_str, cmd + 8, sizeof(dns_str));
    dns_str[sizeof(dns_str) - 1] = '\0';

    if (sscanf(dns_str, "%u.%u.%u.%u", &d0, &d1, &d2, &d3) != 4 || d0 > 255 || d1 > 255 ||
        d2 > 255 || d3 > 255) {
        ERROR_PRINT("Invalid DNS \"%s\"\r\n", dns_str);
        return;
    }

    networkInfo net = LoadUserNetworkConfig();
    net.dns[0] = (uint8_t)d0;
    net.dns[1] = (uint8_t)d1;
    net.dns[2] = (uint8_t)d2;
    net.dns[3] = (uint8_t)d3;

    if (EEPROM_WriteUserNetworkWithChecksum(&net) != 0) {
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

    INFO_PRINT("OK: SET_DNS %s\r\nRebooting...\r\n", dns_str);
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Set the network configuration of the device.
 * This function sets the network configuration including IP, subnet, gateway, and DNS.
 * @param full The full command string.
 * @param cmd The command string containing the network configuration.
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
        ERROR_PRINT("Failed to save network config\r\n");
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

    INFO_PRINT("OK: CONFIG_NETWORK %s$%s$%s$%s\r\nRebooting...\r\n", ip_str, sn_str, gw_str,
               dns_str);
    watchdog_reboot(0, 0, 0);
}

/**
 * @brief Test and print values from HLW8032 channels.
 * This function reads the HLW8032 data for all channels and prints the results.
 * This function iterates through all 8 channels and prints the voltage, current, and power
 * readings.
 */
void test_hlw8032_readings(void) {
    INFO_PRINT("Reading HLW8032 values for all 8 channels...\n");
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (hlw8032_read(ch)) {
            float voltage = hlw8032_get_voltage();
            float current = hlw8032_get_current();
            float power = hlw8032_get_power();
            INFO_PRINT("CH%u: V=%.2f V, I=%.3f A, P=%.2f W\n", ch + 1, voltage, current, power);
        } else {
            ERROR_PRINT("CH%u: Failed to read HLW8032 data\n", ch + 1);
        }
    }
}

/**
 * @brief Test and print values from HLW8032 channels.
 * This function reads the HLW8032 data for a specific channel and prints the results.
 * @param channel Channel index (1–8) to read from.
 */
void test_hlw8032_read_channel(uint8_t channel) {
    INFO_PRINT("Reading HLW8032 values for channel %u...\n", channel);
    if (hlw8032_read(channel)) {
        float voltage = hlw8032_get_voltage();
        float current = hlw8032_get_current();
        float power = hlw8032_get_power();
        INFO_PRINT("CH%u: V=%.2f V, I=%.3f A, P=%.2f W\n", channel, voltage, current, power);
    } else {
        ERROR_PRINT("CH%u: Failed to read HLW8032 data\n", channel);
    }
}
