/**
 * @file src/web_handlers/settings_handler.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui05 5. Settings Handler
 * @ingroup webhandlers
 * @brief Handler for settings page and configuration API endpoints
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 *
 * @details
 * This module handles both the settings HTML page (GET /settings.html) and the settings
 * REST API endpoints (GET/POST /api/settings). It provides configuration management for
 * network parameters and user preferences, with immediate persistence to EEPROM.
 *
 * Supported Operations:
 * - GET /settings.html: Serve gzipped settings page from flash
 * - GET /api/settings: Return current configuration as JSON
 * - POST /api/settings: Update configuration and persist to EEPROM
 *
 * Configuration Categories:
 *
 * Network Settings (via networkInfo struct):
 * - IP address, gateway, subnet mask, DNS server
 * - MAC address (read-only, displayed but not editable)
 * - DHCP enable/disable (future enhancement)
 *
 * User Preferences (via userPrefInfo struct):
 * - Device name (user-friendly identifier)
 * - Location (physical location description)
 * - Temperature unit (Celsius / Fahrenheit / Kelvin)
 *
 * Read-Only Information (displayed in GET /api/settings):
 * - Serial number (from device identity EEPROM block)
 * - Firmware version (compile-time constant)
 * - Hardware version (compile-time constant)
 * - Device region (EU/US from device identity)
 * - Overcurrent protection limits (current_limit, warning, critical thresholds)
 * - Internal temperature (die temperature from ADC)
 *
 * POST Operation Flow:
 * 1. Parse form-encoded POST body (URL decoding applied)
 * 2. Load current network config and preferences from RAM caches
 * 3. Apply updates from form fields
 * 4. Compare with backup to detect changes
 * 5. If network changed: persist to EEPROM and apply to W5500 hardware
 * 6. If preferences changed: persist to EEPROM
 * 7. Send 204 No Content response
 * 8. Trigger StorageTask commit (which may reboot device)
 *
 * Data Persistence:
 * - All configuration stored in EEPROM via StorageTask API
 * - RAM caches maintained by StorageTask for fast access
 * - Changes written immediately but commit may be delayed
 * - POST handler ensures commit before returning (may trigger reboot)
 *
 * Network Configuration Updates:
 * - Settings applied to W5500 hardware immediately after EEPROM write
 * - May cause temporary network disruption during reconfiguration
 * - Browser may lose connection if IP address changes
 *
 * @note All HTTP responses include CORS headers for browser automation.
 * @note POST changes are persistent across reboots.
 * @note Internal temperature reading uses RP2040 die temperature sensor (ADC channel 4).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef SETTINGS_HANDLER_H
#define SETTINGS_HANDLER_H

#include "../CONFIG.h"

/** @name Public API
 * @{
 */
/**
 * @brief Serve settings HTML page via GET /settings.html.
 *
 * Returns the settings page as gzipped HTML from flash memory. Uses chunked transmission
 * (4KB chunks with 5ms delays) to avoid overflowing W5500 TX buffer. The HTML page includes
 * JavaScript that fetches configuration via GET /api/settings and submits changes via POST
 * /api/settings.
 *
 * @param sock Socket number for HTTP connection
 *
 * @return None
 *
 * @http
 * - 200 OK with gzipped HTML body and Content-Encoding: gzip header
 *
 * @note Non-blocking; HTML served directly from flash without dynamic content.
 * @note Client must support gzip encoding (all modern browsers do).
 */
void handle_settings_request(uint8_t sock);

/**
 * @brief Handle GET /api/settings and return configuration as JSON.
 *
 * Aggregates network configuration, user preferences, device identity, overcurrent limits,
 * and internal temperature into a comprehensive JSON response. All data retrieved from
 * RAM caches (network config, preferences) or read-only sources (device identity).
 *
 * JSON Response Fields:
 * - ip, gateway, subnet, dns: Network configuration (dotted-decimal strings)
 * - mac: MAC address (colon-separated hex string, read-only)
 * - device_name: User-configured device name
 * - location: User-configured physical location
 * - temp_unit: Temperature unit preference ("celsius", "fahrenheit", "kelvin")
 * - temperature: Current die temperature in user's preferred unit
 * - serial_number: Device serial number from EEPROM
 * - firmware_version: Firmware version string (compile-time constant)
 * - hardware_version: Hardware version string (compile-time constant)
 * - device_region: Device region ("EU", "US", or "UNKNOWN")
 * - current_limit: Overcurrent limit formatted as "X.X A"
 * - warning_limit: Warning threshold formatted as "X.XX A"
 * - critical_limit: Critical threshold formatted as "X.XX A"
 *
 * Temperature Measurement:
 * - RP2040 die temperature read via ADC channel 4
 * - Converted from raw ADC value using chip-specific formula
 * - Output converted to user's preferred unit
 *
 * @param sock Socket number for HTTP connection
 *
 * @return None
 *
 * @http
 * - 200 OK with JSON body containing all configuration and status fields
 *
 * @note Non-blocking; all data from caches except ADC read for die temperature.
 * @note Die temperature may not reflect actual ambient or case temperature.
 */
void handle_settings_api(uint8_t sock);

/**
 * @brief Handle POST /api/settings to update configuration.
 *
 * Parses form-encoded POST body to extract network settings and user preferences,
 * compares with current values, persists changes to EEPROM via StorageTask, and
 * triggers immediate commit (which may reboot the device). Network changes are
 * applied to W5500 hardware in addition to EEPROM.
 *
 * Form Parameters:
 * - ip: New IP address (dotted-decimal string)
 * - gateway: New gateway address (dotted-decimal string)
 * - subnet: New subnet mask (dotted-decimal string)
 * - dns: New DNS server address (dotted-decimal string)
 * - device_name: New device name (up to 31 chars)
 * - location: New location description (up to 31 chars)
 * - temp_unit: Temperature unit ("celsius", "fahrenheit", "kelvin")
 *
 * Operation:
 * 1. Validate body is present (return 400 if missing)
 * 2. Load current network config and preferences from RAM caches
 * 3. Parse form parameters and apply URL decoding
 * 4. Update network config fields if form values present
 * 5. If network changed: persist to EEPROM and apply to W5500 hardware
 * 6. Update user preferences if form values present
 * 7. If preferences changed: persist to EEPROM
 * 8. Send 204 No Content response
 * 9. Trigger StorageTask commit with 5s timeout (may reboot)
 *
 * Network Reconfiguration:
 * - W5500 hardware reconfigured immediately after EEPROM write
 * - May cause temporary loss of connectivity
 * - If IP address changes, client must reconnect at new address
 *
 * @param sock Socket number for HTTP connection
 * @param body Form-encoded POST body (mutable, decoded in-place)
 *
 * @return None
 *
 * @http
 * - 400 Bad Request if body is NULL
 * - 204 No Content on success (no response body)
 *
 * @note Changes are persistent; stored in EEPROM and survive reboots.
 * @note Commit operation may trigger device reboot for consistency.
 * @note Client connection may be lost if IP address changes.
 */
void handle_settings_post(uint8_t sock, char *body);
/** @} */

#endif // SETTINGS_HANDLER_H

/** @} */