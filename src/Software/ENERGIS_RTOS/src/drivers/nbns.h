/**
 * @file src/drivers/nbns.h
 * @author DvidMakesThings - David Sipos
 *
 * @brief Minimal NetBIOS Name Service (NBNS) responder for the W5500.
 *
 * @details
 * Lets Windows clients resolve the device by its single-label name (e.g.
 * "energis") without mDNS. Windows performs NBNS name resolution over UDP/137
 * broadcast by default, and that traffic is permitted even on "Public" network
 * profiles where mDNS is blocked, so this is the reliable path to reach the unit
 * by name from Windows. Complements the mDNS responder (which serves
 * "<host>.local" to Apple/Linux/Android clients).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef NBNS_H
#define NBNS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize and open the NBNS responder socket.
 *
 * @param sock W5500 socket index (UDP/137).
 * @param name Primary (unique) single-label name (<=15 chars), e.g. "energis-0c77f6".
 * @param alias Optional shared generic name answered in addition, e.g. "energis";
 *              pass NULL/"" to disable.
 * @param ip   Current device IPv4 address (4 bytes).
 * @return true on success, false if the socket could not be opened.
 */
bool NBNS_Init(uint8_t sock, const char *name, const char *alias, const uint8_t ip[4]);

/**
 * @brief Update the IPv4 address returned in future NBNS answers.
 * @param ip New device IPv4 address (4 bytes).
 */
void NBNS_SetIP(const uint8_t ip[4]);

/**
 * @brief Service pending NBNS name queries (non-blocking); answer our name.
 */
void NBNS_Run(void);

#endif /* NBNS_H */
