/**
 * @file src/web_handlers/preset_handler.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui6 6. Preset Handler
 * @ingroup webhandlers
 * @brief HTTP handlers for configuration presets and apply-on-startup
 * @{
 *
 * @version 1.0.0
 * @date 2025-01-01
 *
 * @details
 * Handles HTTP API endpoints for managing configuration presets:
 * - GET/POST /api/config-presets - List, save, delete presets
 * - POST /api/apply-config - Apply a preset immediately
 * - POST /api/startup-config - Set or clear the apply-on-startup preset
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef PRESET_HANDLER_H
#define PRESET_HANDLER_H

#include "../CONFIG.h"

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

#endif /* PRESET_HANDLER_H */

/** @} */