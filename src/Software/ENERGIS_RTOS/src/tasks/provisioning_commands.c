/**
 * @file src/tasks/provisioning_commands.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-12-14
 *
 * @details
 * Implementation of UART command handlers for device provisioning. Provides
 * secure mechanism to set serial number and region on universal firmware.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define PROV_TAG "[PROV]"

/* ==================== Internal Helpers ==================== */

/**
 * @brief Skip leading whitespace.
 *
 * @param s Input string.
 * @return Pointer to first non-whitespace character.
 */
static const char *skip_ws(const char *s) {
    if (!s)
        return "";
    while (*s == ' ' || *s == '\t')
        s++;
    return s;
}

/**
 * @brief Trim leading and trailing whitespace/control chars in-place.
 *
 * @details
 * UART/USB-CDC command lines typically end with "\r\n". The DeviceIdentity
 * validation is intentionally strict and rejects whitespace/control chars.
 * Therefore, provisioning subcommand arguments must be trimmed before use.
 *
 * @param s Mutable string buffer.
 * @return Pointer to trimmed string start (inside the same buffer).
 */
static char *trim_ws_inplace(char *s) {
    if (!s)
        return s;

    /* Trim leading */
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;

    /* Trim trailing */
    size_t len = strlen(s);
    while (len > 0) {
        char c = s[len - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            s[len - 1] = '\0';
            len--;
        } else {
            break;
        }
    }

    return s;
}

/**
 * @brief Extract next token (whitespace-delimited).
 *
 * @param pptr Pointer to string pointer (advanced past token).
 * @return Pointer to token start, or NULL if none.
 */
static char *next_tok(char **pptr) {
    if (!pptr || !*pptr)
        return NULL;

    char *s = (char *)skip_ws(*pptr);
    if (*s == '\0') {
        *pptr = s;
        return NULL;
    }

    char *start = s;
    while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n')
        s++;
    if (*s) {
        *s = '\0';
        s++;
    }
    *pptr = s;
    return start;
}

/* ==================== Subcommand Handlers ==================== */

/**
 * @brief Handle PROV UNLOCK <token> command.
 *
 * @param args Token string.
 */
static void prov_unlock(const char *args) {
    if (!args || *args == '\0') {
        ECHO("%s Usage: PROV UNLOCK <token>\n", PROV_TAG);
        return;
    }

    char tokbuf[64];
    strncpy(tokbuf, args, sizeof(tokbuf) - 1);
    tokbuf[sizeof(tokbuf) - 1] = '\0';
    char *token = trim_ws_inplace(tokbuf);

    if (DeviceIdentity_Unlock(token)) {
        ECHO("%s Provisioning UNLOCKED for %u seconds\n", PROV_TAG, PROV_UNLOCK_TIMEOUT_MS / 1000);
        ECHO("%s Available commands:\n", PROV_TAG);
        ECHO("%s   PROV SET_SN <serial>     - Set serial number\n", PROV_TAG);
        ECHO("%s   PROV SET_REGION <EU|US>  - Set region\n", PROV_TAG);
        ECHO("%s   PROV LOCK                - Lock provisioning\n", PROV_TAG);
    } else {
        ECHO("%s Unlock FAILED: invalid token\n", PROV_TAG);
    }
}

/**
 * @brief Handle PROV LOCK command.
 */
static void prov_lock(void) {
    DeviceIdentity_Lock();
    ECHO("%s Provisioning LOCKED\n", PROV_TAG);
}

/**
 * @brief Handle PROV SET_SN <serial> command.
 *
 * @param args Serial number string.
 */
static void prov_set_sn(const char *args) {
    if (!args || *args == '\0') {
        ECHO("%s Usage: PROV SET_SN <serial>\n", PROV_TAG);
        ECHO("%s   Max length: %d characters\n", PROV_TAG, DEVICE_SN_MAX_LEN);
        ECHO("%s   Allowed: A-Z, a-z, 0-9, hyphen\n", PROV_TAG);
        return;
    }

    char snbuf[64];
    strncpy(snbuf, args, sizeof(snbuf) - 1);
    snbuf[sizeof(snbuf) - 1] = '\0';
    char *sn = trim_ws_inplace(snbuf);

    int ret = DeviceIdentity_SetSerialNumber(sn);
    switch (ret) {
    case 0:
        ECHO("%s Serial number set: %s\n", PROV_TAG, DeviceIdentity_GetSerialNumber());
        ECHO("%s Reboot recommended to apply MAC address\n", PROV_TAG);
        break;
    case -1:
        ECHO("%s FAILED: Provisioning locked. Use PROV UNLOCK first.\n", PROV_TAG);
        break;
    case -2:
        ECHO("%s FAILED: Invalid serial number format\n", PROV_TAG);
        ECHO("%s   Max length: %d characters\n", PROV_TAG, DEVICE_SN_MAX_LEN);
        ECHO("%s   Allowed: A-Z, a-z, 0-9, hyphen\n", PROV_TAG);
        break;
    case -3:
        ECHO("%s FAILED: EEPROM write error\n", PROV_TAG);
        break;
    default:
        ECHO("%s FAILED: Unknown error %d\n", PROV_TAG, ret);
        break;
    }
}

/**
 * @brief Handle PROV SET_REGION <EU|US> command.
 *
 * @param args Region string ("EU" or "US").
 */
static void prov_set_region(const char *args) {
    if (!args || *args == '\0') {
        ECHO("%s Usage: PROV SET_REGION <EU|US>\n", PROV_TAG);
        ECHO("%s   EU: 10A current limit (IEC/ENEC)\n", PROV_TAG);
        ECHO("%s   US: 15A current limit (UL/CSA)\n", PROV_TAG);
        return;
    }

    char regbuf[16];
    strncpy(regbuf, args, sizeof(regbuf) - 1);
    regbuf[sizeof(regbuf) - 1] = '\0';
    char *reg = trim_ws_inplace(regbuf);

    device_region_t region;
    if (strcasecmp(reg, "EU") == 0) {
        region = DEVICE_REGION_EU;
    } else if (strcasecmp(reg, "US") == 0) {
        region = DEVICE_REGION_US;
    } else {
        ECHO("%s FAILED: Invalid region '%s'. Use EU or US.\n", PROV_TAG, reg);
        return;
    }

    int ret = DeviceIdentity_SetRegion(region);
    switch (ret) {
    case 0:
        ECHO("%s Region set: %s (%.0fA limit)\n", PROV_TAG,
             region == DEVICE_REGION_EU ? "EU" : "US", DeviceIdentity_GetCurrentLimitA());
        ECHO("%s Reboot required to apply OCP thresholds\n", PROV_TAG);
        break;
    case -1:
        ECHO("%s FAILED: Provisioning locked. Use PROV UNLOCK first.\n", PROV_TAG);
        break;
    case -2:
        ECHO("%s FAILED: Invalid region code\n", PROV_TAG);
        break;
    case -3:
        ECHO("%s FAILED: EEPROM write error\n", PROV_TAG);
        break;
    default:
        ECHO("%s FAILED: Unknown error %d\n", PROV_TAG, ret);
        break;
    }
}

/**
 * @brief Handle PROV STATUS command.
 */
static void prov_status(void) {
    const device_identity_t *id = DeviceIdentity_Get();

    ECHO("\n=== Device Provisioning Status ===\n");
    ECHO("Serial Number: %s\n", id->serial_number);

    const char *region_str = "UNKNOWN";
    if (id->region == DEVICE_REGION_EU) {
        region_str = "EU (10A)";
    } else if (id->region == DEVICE_REGION_US) {
        region_str = "US (15A)";
    }
    ECHO("Region: %s\n", region_str);
    ECHO("Current Limit: %.1f A\n", DeviceIdentity_GetCurrentLimitA());
    ECHO("Provisioned: %s\n", id->valid ? "Yes" : "No");
    ECHO("Unlock State: %s\n", DeviceIdentity_IsUnlocked() ? "UNLOCKED" : "LOCKED");

    if (DeviceIdentity_IsUnlocked()) {
        ECHO("\nWARNING: Write window is OPEN\n");
    }

    ECHO("==================================\n\n");
}

/* ==================== Public API ==================== */

void cmd_prov(const char *args) {
    if (!args || *args == '\0') {
        /* No subcommand - show help */
        ECHO("\n=== Device Provisioning Commands ===\n");
        ECHO("%-28s %s\n", "PROV UNLOCK <token>", "Unlock provisioning");
        ECHO("%-28s %s\n", "PROV LOCK", "Lock provisioning");
        ECHO("%-28s %s\n", "PROV SET_SN <serial>", "Set serial number");
        ECHO("%-28s %s\n", "PROV SET_REGION <EU|US>", "Set region");
        ECHO("%-28s %s\n", "PROV STATUS", "Show provisioning status");
        ECHO("====================================\n\n");
        return;
    }

    /* Parse subcommand */
    char buf[128];
    strncpy(buf, args, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *ptr = buf;
    char *subcmd = next_tok(&ptr);

    if (!subcmd) {
        ECHO("%s No subcommand. Type PROV for help.\n", PROV_TAG);
        return;
    }

    /* Convert subcommand to uppercase */
    for (char *p = subcmd; *p; p++) {
        *p = (char)toupper((unsigned char)*p);
    }

    /* Get remaining arguments */
    const char *subargs = skip_ws(ptr);

    /* Dispatch subcommand */
    if (strcmp(subcmd, "UNLOCK") == 0) {
        prov_unlock(subargs);
    } else if (strcmp(subcmd, "LOCK") == 0) {
        prov_lock();
    } else if (strcmp(subcmd, "SET_SN") == 0) {
        prov_set_sn(subargs);
    } else if (strcmp(subcmd, "SET_REGION") == 0) {
        prov_set_region(subargs);
    } else if (strcmp(subcmd, "STATUS") == 0) {
        prov_status();
    } else {
        ECHO("%s Unknown subcommand: '%s'. Type PROV for help.\n", PROV_TAG, subcmd);
    }
}
