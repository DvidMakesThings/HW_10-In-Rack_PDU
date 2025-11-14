/**
 * @file src/drivers/websocket.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers09 9. Websocket Implementation
 * @ingroup drivers
 * @brief WebSocket protocol frame handling (RFC 6455)
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-14
 *
 * @details Minimal WebSocket implementation supporting text frames, close frames,
 *          and ping/pong for keep-alive. Used by console WebSocket endpoint.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef WEBSOCKET_PROTOCOL_H
#define WEBSOCKET_PROTOCOL_H

#include "../CONFIG.h"

/* WebSocket opcodes */
#define WS_OP_CONT 0x0
#define WS_OP_TEXT 0x1
#define WS_OP_BINARY 0x2
#define WS_OP_CLOSE 0x8
#define WS_OP_PING 0x9
#define WS_OP_PONG 0xA

/* WebSocket frame structure */
typedef struct {
    uint8_t opcode;
    bool fin;
    bool masked;
    uint8_t mask[4];
    uint64_t payload_len;
    uint8_t *payload;
} ws_frame_t;

/**
 * @brief Generates WebSocket accept key from client key
 * @param client_key Client's Sec-WebSocket-Key header value
 * @param accept_key Output buffer (min 29 bytes)
 * @return true on success, false on failure
 */
bool ws_generate_accept_key(const char *client_key, char *accept_key);

/**
 * @brief Parses WebSocket frame from buffer
 * @param buf Input buffer
 * @param len Buffer length
 * @param frame Output frame structure
 * @return Number of bytes consumed, or -1 on error, 0 if incomplete
 */
int ws_parse_frame(const uint8_t *buf, int len, ws_frame_t *frame);

/**
 * @brief Encodes WebSocket frame to buffer
 * @param opcode Frame opcode
 * @param payload Payload data
 * @param payload_len Payload length
 * @param buf Output buffer
 * @param buflen Buffer size
 * @return Number of bytes written, or -1 on error
 */
int ws_encode_frame(uint8_t opcode, const uint8_t *payload, uint16_t payload_len, uint8_t *buf,
                    int buflen);

/**
 * @brief Unmasks WebSocket payload in-place
 * @param payload Payload buffer
 * @param len Payload length
 * @param mask Mask key (4 bytes)
 */
void ws_unmask_payload(uint8_t *payload, uint64_t len, const uint8_t mask[4]);

#endif // WEBSOCKET_PROTOCOL_H

/** @} */