/**
 * @file src/web_handlers/preset_handler.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui6 6. Preset Handler
 * @ingroup webhandlers
 * @brief HTTP handlers for configuration preset management and apply-on-startup
 * @{
 *
 * @version 1.0.0
 * @date 2025-01-01
 *
 * @details
 * This module provides REST API endpoints for managing relay configuration presets.
 * Users can save current relay states as named presets, recall them instantly, and
 * configure a preset to apply automatically on device boot.
 *
 * Supported Endpoints:
 * - GET /api/config-presets: List all saved presets and current startup selection
 * - POST /api/config-presets: Save or delete a preset
 * - POST /api/apply-config: Apply a preset immediately to relay outputs
 * - POST /api/startup-config: Set or clear the apply-on-startup preset
 *
 * Preset Storage:
 * - Up to 5 presets (USER_OUTPUT_MAX_PRESETS = 5)
 * - Each preset stores: name (up to 25 chars), 8-bit relay mask (0-255)
 * - Stored in EEPROM via StorageTask for persistence across reboots
 * - RAM cache maintained by StorageTask for fast access
 *
 * Relay Mask Encoding:
 * - Each bit represents one relay channel (bit 0 = channel 1, bit 7 = channel 8)
 * - 1 = ON, 0 = OFF
 * - Example: 0b00001111 (15) = channels 1-4 ON, 5-8 OFF
 *
 * Apply-on-Startup Feature:
 * - One preset can be designated as the "startup preset"
 * - If set, this preset applies automatically during InitTask
 * - Ensures consistent power state after reboot or power cycle
 * - Can be cleared to disable automatic application
 *
 * Overcurrent Protection Integration:
 * - Apply operations check Overcurrent_IsSwitchingAllowed() before switching
 * - Returns 503 Service Unavailable if overcurrent lockout is active
 * - Prevents potentially unsafe relay operations during overcurrent conditions
 *
 * Thread Safety:
 * - All EEPROM operations routed through StorageTask (eepromMtx protected)
 * - All HTTP handlers called from NetTask context
 * - Relay operations dispatched to SwitchTask via non-blocking queue
 *
 * @note All responses include CORS headers for browser-based automation.
 * @note Preset names are JSON-escaped for safety.
 * @note Changes are persistent; stored in EEPROM and survive reboots.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef PRESET_HANDLER_H
#define PRESET_HANDLER_H

#include "../CONFIG.h"

/** @name Public API
 * @{
 */
/**
 * @brief Handle GET request for /api/config-presets
 *
 * Returns JSON with all presets and current startup selection:
 * {
 *   "presets": [
 *     {"id": 0, "name": "...", "mask": 0},
 *     ...
 *   ],
 *   "startup": null or 0-4
 * }
 *
 * @param sock Socket number.
 */
void handle_config_presets_get(uint8_t sock);

/**
 * @brief Handle POST request for /api/config-presets
 *
 * Accepts form-encoded body with actions:
 * - action=save&id=N&name=...&mask=N  -> Save preset at slot N
 * - action=delete&id=N                 -> Delete preset at slot N
 *
 * @param sock Socket number.
 * @param body Form-encoded POST body.
 */
void handle_config_presets_post(uint8_t sock, char *body);

/**
 * @brief Handle POST request for /api/apply-config
 *
 * Accepts form-encoded body: id=N
 * Applies the preset immediately to relay outputs.
 *
 * @param sock Socket number.
 * @param body Form-encoded POST body.
 */
void handle_apply_config_post(uint8_t sock, char *body);

/**
 * @brief Handle POST request for /api/startup-config
 *
 * Accepts form-encoded body:
 * - id=N          -> Set startup preset to N
 * - action=clear  -> Clear startup preset
 *
 * @param sock Socket number.
 * @param body Form-encoded POST body.
 */
void handle_startup_config_post(uint8_t sock, char *body);
/** @} */

#endif /* PRESET_HANDLER_H */

/** @} */