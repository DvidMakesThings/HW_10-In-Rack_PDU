/**
 * @file src/web_handlers/console_websocket.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup webui6 6. Console WebSocket Handler
 * @ingroup webhandlers
 * @brief WebSocket endpoint for secure console access
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-14
 *
 * @details Provides WebSocket endpoint at /ws/console for authenticated console
 *          access. Password is hashed (SHA-256) in browser and compared against
 *          stored hash. Bridges console I/O between USB-CDC and WebSocket clients.
 *          Automatic logout after 5 minutes of inactivity.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CONSOLE_WEBSOCKET_H
#define CONSOLE_WEBSOCKET_H

#include "../CONFIG.h"

/* Console WebSocket configuration */
#define CONSOLE_WS_SOCKET 3
#define CONSOLE_WS_TIMEOUT_MS (5 * 60 * 1000) /* 5 minutes */
#define CONSOLE_WS_BUF_SIZE 512

/**
 * @brief Computes SHA-256 hash of input data (exposed for console command)
 * @param data Input data buffer
 * @param len Length of input data in bytes
 * @param hash Output hash buffer (32 bytes)
 */
void sha256_compute(const uint8_t *data, size_t len, uint8_t hash[32]);

/**
 * @brief Initializes console WebSocket subsystem
 * @return true on success, false on failure
 * @note Call once during NetTask initialization
 */
bool console_ws_init(void);

/**
 * @brief Handles WebSocket upgrade request for /ws/console
 * @param sock HTTP socket with upgrade request
 * @param headers Request headers
 * @param query Query string (contains key parameter)
 * @return true if upgrade successful, false if rejected
 *
 * @details Validates SHA-256 hash from ?key= parameter against stored hash.
 *          If valid, performs WebSocket handshake and takes ownership of socket.
 */
bool console_ws_handle_upgrade(uint8_t sock, const char *headers, const char *query);

/**
 * @brief Processes console WebSocket events (call from NetTask loop)
 * @return None
 *
 * @details Handles incoming WebSocket frames, forwards to console,
 *          sends console output to client, implements timeout.
 */
void console_ws_process(void);

/**
 * @brief Checks if console WebSocket is active
 * @return true if client connected, false otherwise
 */
bool console_ws_is_active(void);

/**
 * @brief Sends text to console WebSocket client
 * @param text Text string to send
 * @param len Text length
 * @return true if sent, false if no client or send failed
 */
bool console_ws_send_text(const char *text, int len);

#endif // CONSOLE_WEBSOCKET_H

/** @} */