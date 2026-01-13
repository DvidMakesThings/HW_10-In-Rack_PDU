/**
 * @file src/snmp/snmp_networkCtrl.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.0
 * @date 2025-11-08
 *
 * @details
 * Implementation of SNMP network configuration getters. This module reads the
 * persisted network configuration from EEPROM via StorageTask and formats
 * network addresses as human-readable strings for SNMP responses.
 *
 * Formatting conventions:
 * - IPv4 addresses: dotted-decimal (e.g., "192.168.1.100")
 * - MAC addresses: colon-separated hex (e.g., "02:45:4E:0C:7B:1C")
 *
 * All functions include error handling with automatic fallback to compiled
 * defaults if storage access fails.
 */

#include "../CONFIG.h"

#define SNMPNETCTL_TAG "[SNMPNCT]"

/* ===== Internal: fetch persisted network config (EEPROM) ===== */

/**
 * @brief Load persisted network configuration from EEPROM.
 *
 * Attempts to read the network configuration from persistent storage via
 * storage_get_network(). If the read fails, populates the output structure
 * with compiled-in defaults.
 *
 * @param out Pointer to destination networkInfo structure
 *
 * @return None
 *
 * @note Validates output pointer and logs error if NULL.
 */
static inline void load_netcfg(networkInfo *out) {
    /* Validate output pointer */
    if (!out) {
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_SNMP_NETCTRL, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s Null pointer in load netconfig\n", SNMPNETCTL_TAG);
        Storage_EnqueueErrorCode(errorcode);
        return;
    }

    /* Attempt to read from storage; fall back to defaults on failure */
    if (!storage_get_network(out)) {
        *out = LoadUserNetworkConfig();
    }
}

/**
 * @brief Format IPv4 address as dotted-decimal string.
 *
 * Converts a 4-byte IPv4 address to the standard dotted-decimal notation
 * with null termination.
 *
 * @param ptr Output buffer (minimum 16 bytes required for "255.255.255.255\0")
 * @param ip Source IPv4 address as 4-byte array
 *
 * @return Number of characters written, excluding null terminator
 */
static inline uint8_t ipfmt(void *ptr, const uint8_t ip[4]) {
    return (uint8_t)snprintf((char *)ptr, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

/**
 * @brief Format MAC address as colon-separated hexadecimal string.
 *
 * Converts a 6-byte MAC address to the standard colon-separated uppercase
 * hexadecimal format with null termination.
 *
 * @param ptr Output buffer (minimum 18 bytes required for "FF:FF:FF:FF:FF:FF\0")
 * @param mac Source MAC address as 6-byte array
 *
 * @return Number of characters written, excluding null terminator
 */
static inline uint8_t macfmt(void *ptr, const uint8_t mac[6]) {
    return (uint8_t)snprintf((char *)ptr, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
                             mac[2], mac[3], mac[4], mac[5]);
}

/* ===== SNMP getters (string-typed) ===== */

void get_networkIP(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = ipfmt(ptr, ni.ip);
}

void get_networkMask(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = ipfmt(ptr, ni.sn);
}

void get_networkGateway(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = ipfmt(ptr, ni.gw);
}

void get_networkDNS(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = ipfmt(ptr, ni.dns);
}

void get_networkMAC(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = macfmt(ptr, ni.mac);
}
