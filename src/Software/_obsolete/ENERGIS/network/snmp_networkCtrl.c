/**
 * @file snmp_networkCtrl.c
 * @author DvidMakesThings - David Sipos
 * @defgroup snmp3 3. SNMP Agent - Network Configuration
 * @ingroup snmp
 * @brief Custom SNMP agent implementation for ENERGIS PDU network configuration.
 * @{
 * @version 1.0
 * @date 2025-05-17
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "snmp_networkCtrl.h"
#include "CONFIG.h"
#include "network/wizchip_conf.h" // wiz_NetInfo

/**
 * @brief Bring in the running network configuration
 * @return Pointer to the network configuration structure.
 */
extern wiz_NetInfo g_net_info;

// ####################################################################################
//  Network callbacks: format each 4‐byte field into a dotted‐decimal string
// ####################################################################################

/**
 * @brief Get the IP address of the device.
 * @param ptr Pointer to the buffer to store the IP address.
 * @param len Pointer to the length of the IP address.
 */
void get_networkIP(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u", g_net_info.ip[0], g_net_info.ip[1], g_net_info.ip[2],
                    g_net_info.ip[3]);
}

/**
 * @brief Get the subnet mask of the device.
 * @param ptr Pointer to the buffer to store the subnet mask.
 * @param len Pointer to the length of the subnet mask.
 */
void get_networkMask(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u", g_net_info.sn[0], g_net_info.sn[1], g_net_info.sn[2],
                    g_net_info.sn[3]);
}

/**
 * @brief Get the gateway address of the device.
 * @param ptr Pointer to the buffer to store the gateway address.
 * @param len Pointer to the length of the gateway address.
 */
void get_networkGateway(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u", g_net_info.gw[0], g_net_info.gw[1], g_net_info.gw[2],
                    g_net_info.gw[3]);
}

/**
 * @brief Get the DNS server address of the device.
 * @param ptr Pointer to the buffer to store the DNS server address.
 * @param len Pointer to the length of the DNS server address.
 */
void get_networkDNS(void *ptr, uint8_t *len) {
    char *dst = (char *)ptr;
    *len = snprintf(dst, 16, "%u.%u.%u.%u", g_net_info.dns[0], g_net_info.dns[1], g_net_info.dns[2],
                    g_net_info.dns[3]);
}

/** @} */ // End of snmp3