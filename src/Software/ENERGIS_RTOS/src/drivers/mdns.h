/**
 * @file src/drivers/mdns.h
 * @author DvidMakesThings - David Sipos
 *
 * @brief Minimal multicast-DNS (Bonjour/Avahi) responder for the W5500.
 *
 * @details
 * Answers link-local "<hostname>.local" A-record queries so the device is
 * reachable by name even when its IPv4 address is unknown (e.g. after DHCP on a
 * fresh network). Runs on a dedicated UDP multicast socket (224.0.0.251:5353)
 * and is polled cooperatively from NetTask.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef MDNS_H
#define MDNS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize and open the mDNS responder socket.
 *
 * @param sock     W5500 socket index (UDP multicast).
 * @param hostname Primary (unique) host label, e.g. "energis-0c77f6".
 * @param alias    Optional shared generic label answered in addition, e.g.
 *                 "energis"; pass NULL/"" to disable.
 * @param ip       Current device IPv4 address (4 bytes) advertised in answers.
 * @return true on success, false if the multicast socket could not be opened.
 */
bool mDNS_Init(uint8_t sock, const char *hostname, const char *alias, const uint8_t ip[4]);

/**
 * @brief Update the IPv4 address advertised in future answers.
 *
 * @param ip New device IPv4 address (4 bytes). Safe to call on every IP change.
 */
void mDNS_SetIP(const uint8_t ip[4]);

/**
 * @brief Service pending mDNS queries (non-blocking); answer matching A queries.
 */
void mDNS_Run(void);

/**
 * @brief Send an unsolicited announcement for the current name/IP.
 *
 * Useful right after the address changes so caches update promptly.
 */
void mDNS_Announce(void);

#endif /* MDNS_H */
