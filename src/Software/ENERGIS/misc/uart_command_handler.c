/**
 * @file uart_command_handler.c
 * @brief UART command handler over stdio (USB-CDC) for ENERGIS PDU.
 *
 * Reads lines from stdin (your CDC serial port), matches them against
 * known commands, and sends responses back via printf().
 */

#include "CONFIG.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

extern volatile uint32_t bootloader_trigger;
extern wiz_NetInfo g_net_info; // current network info in RAM

//------------------------------------------------------------------------------
// Helper: trim leading and trailing whitespace in place.
// Returns pointer to the first non-space character.
// Helper: trim leading/trailing whitespace in place.
static char *trim(char *s) {
    char *end;
    while (*s && isspace((unsigned char)*s))
        s++;
    if (*s == '\0')
        return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        *end-- = '\0';
    return s;
}

//------------------------------------------------------------------------------
// process_command(): compare a null–terminated, trimmed command string
// and perform the associated action.
static void process_command(const char *cmd) {
    if (strcmp(cmd, "HELP") == 0) {
        printf("\nAvailable commands:\r\n"
               "HELP\r\n"
               "BOOTSEL\r\n"
               "DUMP_EEPROM\r\n"
               "REBOOT\r\n"
               "SET_IP <addr>\r\n"
               "NETINFO\r\n");
    } else if (strcmp(cmd, "BOOTSEL") == 0) {
        bootsel_trigger();
    } else if (strcmp(cmd, "REBOOT") == 0) {
        reboot();
    } else if (strcmp(cmd, "DUMP_EEPROM") == 0) {
        dump_eeprom();
    } else if (strncmp(cmd, "SET_IP ", 7) == 0) {
        set_ip(cmd + 7, cmd);
    } else if (strncmp(cmd, "NETINFO", 9) == 0) {
        INFO_PRINT("NETWORK INFORMATION:\r\n");
        print_network_information(g_net_info);
    } else {
        // unrecognized command
        ERROR_PRINT("Unknown command \"%s\"\r\n", cmd);
    }
}

//------------------------------------------------------------------------------
// uart_command_loop(): blocks on fgets(), dispatches each entered line.
// Call this once (it never returns) after stdio_init_all()/stdio_usb_init().
void uart_command_loop(void) {
    static char line[UART_CMD_BUF_LEN];

    while (fgets(line, sizeof(line), stdin)) {
        // Strip trailing newline or carriage return
        size_t len = strlen(line);
        if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        // Trim spaces and skip empty
        char *cmd = trim(line);
        if (*cmd == '\0') {
            continue;
        }

        // Dispatch
        process_command(cmd);
    }
}

void bootsel_trigger(void) {
    // trigger BOOTSEL on next reboot
    bootloader_trigger = 0xDEADBEEF;
    INFO_PRINT("OK: ENTERING BOOTSEL\r\n");
    watchdog_reboot(0, 0, 0);
}

void reboot(void) {
    INFO_PRINT("OK: REBOOTING\r\n");
    watchdog_reboot(0, 0, 0);
}

void dump_eeprom(void) {
    // dump EEPROM, then acknowledge
    CAT24C512_DumpFormatted();
    INFO_PRINT("OK: DUMP_EEPROM\r\n");
}

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