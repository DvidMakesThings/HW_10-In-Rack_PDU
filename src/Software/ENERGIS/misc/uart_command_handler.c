/**
 * @file uart_command_handler.c
 * @author David Sipos
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
    char cmd[32];
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
    } else if (strcmp(trimmed, "NETINFO") == 0) {
        INFO_PRINT("NETWORK INFORMATION:\r\n");
        print_network_information(g_net_info);
    } else if (strcmp(trimmed, "SYSINFO") == 0) {
        INFO_PRINT("SYSTEM INFORMATION:\r\n");
        getSysInfo();
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

    // Flush any garbage
    while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT)
        ;

    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        char *cmd = trim(line);
        if (*cmd == '\0')
            continue;

        process_command(cmd);
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
    INFO_PRINT("SYS: %u Hz\nUSB: %u Hz\nPERI: %u Hz\nADC: %u Hz\n", sys_freq, usb_freq, peri_freq,
               adc_freq);
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