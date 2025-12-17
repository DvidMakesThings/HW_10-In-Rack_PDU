/**
 * @file src/snmp/snmp_networkCtrl.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.1.0
 * @date 2025-11-08
 *
 * @details
 * Reads the user/network configuration from EEPROM using storage helpers and
 * formats fields for SNMP. If EEPROM access fails, falls back to compiled defaults.
 */

#include "../CONFIG.h"

#define SNMPNETCTL_TAG "[SNMPNCT]"

/* ===== Internal: fetch persisted network config (EEPROM) ===== */

/**
 * @brief Load the persisted network configuration into a caller-provided struct.
 *
 * @param[out] out  Destination struct to receive the persisted configuration.
 *
 * @post On success, @p out fields are populated from EEPROM. On failure, @p out
 *       receives compiled defaults via LoadUserNetworkConfig().
 */
static inline void load_netcfg(networkInfo *out) {
    if (!out) {
        uint16_t errorcode =
            ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR, ERR_FID_NET_SNMP_NETCTRL, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s Null pointer in load netconfig\n", SNMPNETCTL_TAG);
        Storage_EnqueueErrorCode(errorcode);
        return;
    }
    if (!storage_get_network(out)) {
        *out = LoadUserNetworkConfig();
    }
}

/**
 * @brief Format an IPv4 address to dotted decimal.
 *
 * @param[out] ptr  Destination buffer (must be at least 16 bytes).
 * @param[in]  ip   Source IPv4 address as 4 octets.
 *
 * @return Number of characters written excluding the null terminator.
 *
 * @note The output format is "x.x.x.x" and is null-terminated.
 */
static inline uint8_t ipfmt(void *ptr, const uint8_t ip[4]) {
    return (uint8_t)snprintf((char *)ptr, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

/**
 * @brief Format a MAC address to colon-separated hex.
 *
 * @param[out] ptr  Destination buffer (must be at least 18 bytes).
 * @param[in]  mac  Source MAC address as 6 octets.
 *
 * @return Number of characters written excluding the null terminator.
 *
 * @note The output format is "02:45:4E:0C:7B:1C" and is null-terminated.
 */
static inline uint8_t macfmt(void *ptr, const uint8_t mac[6]) {
    return (uint8_t)snprintf((char *)ptr, 18, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
                             mac[2], mac[3], mac[4], mac[5]);
}

/* ===== SNMP getters (string-typed) ===== */

/**
 * @brief Get persisted IP address (EEPROM) for SNMP.
 *
 * @param[out] ptr  Output buffer for the string (>=16 bytes recommended).
 * @param[out] len  Number of characters written to @p ptr (excluding null).
 *
 * @pre Storage helpers are initialized.
 * @post @p ptr contains the dotted-decimal IP; @p len reflects byte count.
 */
void get_networkIP(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = ipfmt(ptr, ni.ip);
}

/**
 * @brief Get persisted subnet mask (EEPROM) for SNMP.
 *
 * @param[out] ptr  Output buffer for the string (>=16 bytes recommended).
 * @param[out] len  Number of characters written to @p ptr (excluding null).
 *
 * @pre Storage helpers are initialized.
 * @post @p ptr contains the dotted-decimal mask; @p len reflects byte count.
 */
void get_networkMask(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = ipfmt(ptr, ni.sn);
}

/**
 * @brief Get persisted gateway (EEPROM) for SNMP.
 *
 * @param[out] ptr  Output buffer for the string (>=16 bytes recommended).
 * @param[out] len  Number of characters written to @p ptr (excluding null).
 *
 * @pre Storage helpers are initialized.
 * @post @p ptr contains the dotted-decimal gateway; @p len reflects byte count.
 */
void get_networkGateway(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = ipfmt(ptr, ni.gw);
}

/**
 * @brief Get persisted DNS server (EEPROM) for SNMP.
 *
 * @param[out] ptr  Output buffer for the string (>=16 bytes recommended).
 * @param[out] len  Number of characters written to @p ptr (excluding null).
 *
 * @pre Storage helpers are initialized.
 * @post @p ptr contains the dotted-decimal DNS address; @p len reflects byte count.
 */
void get_networkDNS(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = ipfmt(ptr, ni.dns);
}

/**
 * @brief Get persisted MAC address (EEPROM) for SNMP.
 *
 * @param[out] ptr  Output buffer for the string (>=16 bytes recommended).
 * @param[out] len  Number of characters written to @p ptr (excluding null).
 *
 * @pre Storage helpers are initialized.
 * @post @p ptr contains the dotted-decimal MAC address; @p len reflects byte count.
 */
void get_networkMAC(void *ptr, uint8_t *len) {
    networkInfo ni;
    load_netcfg(&ni);
    *len = macfmt(ptr, ni.mac);
}
