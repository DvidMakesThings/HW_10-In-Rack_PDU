/**
 * @file src/drivers/dhcp.h
 * @author DvidMakesThings - David Sipos
 *
 * @brief Minimal DHCPv4 client for the W5500 (application-level, no lwIP).
 *
 * @details
 * Implements a compact DORA (Discover/Offer/Request/Ack) client on top of the
 * project's W5500 socket API. The client runs on a dedicated UDP socket and is
 * driven cooperatively from NetTask. All internal wait loops feed the health
 * watchdog so an acquisition never starves the system.
 *
 * Usage:
 *   DHCP_Init(DHCP_SOCKET_NUM, mac);
 *   if (DHCP_Acquire(ip, sn, gw, dns, &lease_s)) { apply ip... }
 *   // periodically, when the lease timer elapses, call DHCP_Acquire() again.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef DHCP_H
#define DHCP_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize the DHCP client.
 *
 * @param sock W5500 socket index to use for DHCP (UDP/68).
 * @param mac  Pointer to the 6-byte client hardware (MAC) address.
 * @param hostname Host name to advertise (DHCP option 12), or NULL to omit.
 */
void DHCP_Init(uint8_t sock, const uint8_t *mac, const char *hostname);

/**
 * @brief Perform a bounded DHCP acquisition (DISCOVER -> OFFER -> REQUEST -> ACK).
 *
 * Blocks cooperatively (feeding the NET heartbeat) for at most
 * DHCP_RETRY_MAX * 2 * DHCP_STEP_TIMEOUT_MS. On success the leased parameters are
 * written to the output buffers and the W5500 SIPR/SUBR/GAR registers are set to
 * the leased address so networking is immediately live.
 *
 * @param ip       Out: leased IPv4 address (4 bytes).
 * @param sn       Out: subnet mask (4 bytes).
 * @param gw       Out: gateway (4 bytes).
 * @param dns      Out: DNS server (4 bytes, zeroed if not offered).
 * @param lease_s  Out: lease time in seconds (0 if not offered).
 * @return true if a lease was obtained, false on timeout/failure.
 */
bool DHCP_Acquire(uint8_t ip[4], uint8_t sn[4], uint8_t gw[4], uint8_t dns[4], uint32_t *lease_s);

/**
 * @brief Query whether a valid lease is currently held.
 * @return true if bound to a DHCP lease.
 */
bool DHCP_IsBound(void);

/**
 * @brief Release internal bound state (e.g. when switching back to static).
 */
void DHCP_Reset(void);

#endif /* DHCP_H */
