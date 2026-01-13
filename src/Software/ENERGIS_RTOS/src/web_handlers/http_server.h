/**
 * @file src/web_handlers/http_server.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui02 2. HTTP Server
 * @ingroup webhandlers
 * @brief Main HTTP server with chunked transfer for large pages
 * @{
 *
 * @version 1.1.0
 * @date 2025-01-01
 *
 * @details
 * This module implements the core HTTP/1.1 server for the ENERGIS managed PDU.
 * It provides web-based access to device control, monitoring, and configuration
 * through a responsive HTML interface and JSON REST API.
 *
 * Architecture:
 * - Single-socket TCP listener on port 80
 * - Request parsing with automatic Content-Length handling
 * - Chunked transmission to work within W5500 8KB TX buffer limit
 * - Route-based dispatch to specialized handlers
 * - CORS headers for cross-origin API access
 * - Connection timeout detection and recovery
 *
 * Routing Table:
 * - GET  /api/status              -> Status JSON (channel states, telemetry, OCP)
 * - GET  /api/settings            -> Settings JSON (network config, preferences)
 * - POST /api/settings            -> Update settings, trigger reboot
 * - POST /api/control             -> Control relays and labels
 * - GET  /api/config-presets      -> List saved presets
 * - POST /api/config-presets      -> Save/delete preset
 * - POST /api/apply-config        -> Apply preset immediately
 * - POST /api/startup-config      -> Set/clear apply-on-startup preset
 * - GET  /metrics                 -> Prometheus/OpenMetrics telemetry
 * - GET  /settings.html           -> Settings page (gzipped)
 * - GET  /help.html               -> Help page (gzipped)
 * - GET  /control.html, GET /     -> Control page (gzipped, default)
 * - GET  /user_manual.html        -> User manual (iframe embed)
 * - GET  /automation_manual.html  -> Automation manual (iframe embed)
 *
 * Constraints:
 * - W5500 has 8KB TX buffer per socket
 * - HTML pages can exceed 8KB (especially help.html)
 * - Solution: Gzip compression + chunked sending (4KB chunks with delays)
 * - RX buffer allocated once at init: 2KB
 *
 * Thread Safety:
 * - All W5500 access protected by spiMtx (acquired by NetTask)
 * - Called only from NetTask context
 * - Handlers may access shared resources (require their own locking)
 *
 * Connection Management:
 * - Single-connection model (one request at a time)
 * - After response: disconnect, close, re-open, re-listen
 * - TX watchdog: if peer stalls (TX FSR stuck for >1.2s), forcibly close
 *
 * @note Uses synchronous blocking patterns within NetTask; OK as this is
 *       the only task performing network operations.
 * @note CORS enabled for all responses to allow browser-based automation.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "../CONFIG.h"

/** @name Public API
 * @{
 */
/**
 * @brief Initialize the HTTP server and start listening on port 80.
 *
 * Allocates the HTTP request buffer (2KB) and opens a TCP socket on port 80.
 * Configures TCP keep-alive and begins listening for incoming connections.
 * Must be called once during system initialization before http_server_process().
 *
 * @return true on successful initialization, false on failure
 *
 * @retval true Socket opened, buffer allocated, listening started
 * @retval false Failed to allocate buffer, open socket, or start listening
 *
 * @note Logs errors with structured error codes on failure.
 * @note Uses heap allocation for RX buffer via pvPortMalloc().
 */
bool http_server_init(void);

/**
 * @brief Process incoming HTTP connections and route requests to handlers.
 *
 * This function must be called repeatedly from NetTask's main loop. It checks
 * the socket state, reads incoming HTTP requests, dispatches to appropriate
 * handlers based on URL patterns, and manages connection lifecycle.
 *
 * Operation:
 * - Monitors socket state (ESTABLISHED, CLOSE_WAIT, INIT)
 * - Reads HTTP headers and body using Content-Length
 * - Routes requests to handler functions
 * - Implements TX watchdog (forcibly closes stalled connections after 1.2s)
 * - Closes and re-listens after each response
 *
 * @return None
 *
 * @note Blocking within NetTask context; safe as NetTask is single-threaded.
 * @note TX watchdog protects against slow/stalled HTTP clients.
 * @note All responses include "Connection: close" header.
 */
void http_server_process(void);

/** @} */

#endif // HTTP_SERVER_H

/** @} */