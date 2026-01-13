/**
 * @file src/web_handlers/status_handler.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui06 6. Status Handler
 * @ingroup webhandlers
 * @brief Handler for GET /api/status endpoint providing system telemetry
 * @{
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details
 * This module handles GET requests to /api/status and returns a comprehensive JSON response
 * containing all system telemetry and status information. The handler aggregates data from
 * multiple subsystems using cached values to provide immediate responses without blocking
 * on hardware measurements.
 *
 * Data Sources:
 * - Channel states: Cached from SwitchTask (updated in real-time by relay control)
 * - Power metrics: Cached from MeterTask (updated periodically by HLW8032 measurements)
 * - Channel labels: Retrieved from RAM cache (synchronized with EEPROM by StorageTask)
 * - Internal temperature: Cached die temperature from MeterTask (updated with power metrics)
 * - Overcurrent status: Real-time status from Overcurrent Protection module
 *
 * Architecture Benefits:
 * - Non-blocking: All data retrieved from caches; no waiting for I2C, SPI, or UART
 * - Fast response: Typical response time <10ms regardless of measurement cadence
 * - Consistency: All readings timestamped from same source (task-maintained caches)
 * - Decoupling: UI refresh rate independent of hardware measurement intervals
 *
 * JSON Response Structure:
 * @code{.json}
 * {
 *   "channels": [
 *     {
 *       "voltage": 230.5,
 *       "current": 0.45,
 *       "uptime": 3600,
 *       "power": 103.7,
 *       "state": true,
 *       "label": "Web Server"
 *     },
 *     ...
 *   ],
 *   "internalTemperature": 45.2,
 *   "temperatureUnit": "°C",
 *   "systemStatus": "OK",
 *   "overcurrent": {
 *     "state": "NORMAL",
 *     "total_current_a": 3.6,
 *     "limit_a": 10.0,
 *     "warning_threshold_a": 8.0,
 *     "critical_threshold_a": 9.5,
 *     "recovery_threshold_a": 7.5,
 *     "switching_allowed": true,
 *     "trip_count": 0,
 *     "region": "EU"
 *   }
 * }
 * @endcode
 *
 * Temperature Unit Handling:
 * - User preference stored in EEPROM (0=Celsius, 1=Fahrenheit, 2=Kelvin)
 * - Die temperature measured in Celsius, converted based on preference
 * - Unit string included in response for client-side display
 *
 * System Status Determination:
 * - "OK": Normal operation (OC_STATE_NORMAL)
 * - "WARNING": Elevated current or critical threshold reached
 * - "LOCKOUT": Overcurrent protection triggered, switching disabled
 *
 * @note All responses include CORS headers for browser-based automation.
 * @note JSON strings are escaped for safety (quotes, backslashes, control chars).
 * @note Called from NetTask context; all data access is thread-safe via caches.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef STATUS_HANDLER_H
#define STATUS_HANDLER_H

#include "../CONFIG.h"

/** @name Public API
 * @{
 */
/**
 * @brief Handle GET /api/status endpoint and return JSON telemetry.
 *
 * Aggregates system status from multiple cached sources and formats a comprehensive
 * JSON response containing all channel states, power measurements, labels, temperature,
 * and overcurrent protection status. All data is retrieved from RAM caches maintained
 * by other tasks, ensuring fast non-blocking response regardless of hardware timing.
 *
 * Operation:
 * 1. Retrieve cached die temperature from MeterTask
 * 2. Load user preference for temperature unit (Celsius/Fahrenheit/Kelvin)
 * 3. Snapshot all 8 channel relay states from SwitchTask cache
 * 4. Retrieve overcurrent protection status and determine system status
 * 5. Load all channel labels from RAM cache (EEPROM-synchronized)
 * 6. Build JSON array with per-channel telemetry (voltage, current, power, uptime, state, label)
 * 7. Append temperature, system status, and overcurrent details to JSON
 * 8. Send HTTP 200 OK response with JSON body and CORS headers
 *
 * JSON Safety:
 * - Channel labels are JSON-escaped to handle quotes, backslashes, and control chars
 * - All numeric values formatted with fixed precision
 * - Boolean values rendered as true/false literals
 *
 * @param sock Socket number for HTTP connection
 *
 * @return None
 *
 * @http
 * - 200 OK: Success with JSON body containing complete system status
 *
 * @note Non-blocking; all data from caches, no hardware access.
 * @note Called from NetTask; assumes SPI mutex already held.
 * @note Response includes Connection: close and Cache-Control: no-cache headers.
 */
void handle_status_request(uint8_t sock);
/** @} */

#endif // STATUS_HANDLER_H

/** @} */