/**
 * @file src/web_handlers/console_websocket.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.1
 * @date 2025-11-14
 *
 * @details Provides WebSocket endpoint at /ws/console for authenticated console
 *          access. Password is sent as plaintext via query parameter, hashed on MCU,
 *          and compared against stored hash. Bridges console I/O between USB-CDC
 *          and WebSocket clients.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

#define CONSOLE_WS_TAG "[ConsoleWS]"

/**
 * @brief WebSocket state structure
 */
static struct {
    int8_t sock;               /**< Socket number, -1 if inactive */
    bool active;               /**< Connection active flag */
    uint32_t last_activity_ms; /**< Last activity timestamp for timeout */
    uint8_t *rx_buf;           /**< Receive buffer */
    uint8_t *tx_buf;           /**< Transmit buffer */
} ws_state = {.sock = -1, .active = false, .last_activity_ms = 0};

/* ==================== SHA-256 Implementation ==================== */

/**
 * @brief SHA-256 rotation and logic macros
 */
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

/**
 * @brief SHA-256 round constants (first 32 bits of fractional parts of cube roots of first 64
 * primes)
 */
static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

/**
 * @brief SHA-256 transformation function (processes one 64-byte block)
 * @param state Current hash state (8 x 32-bit words)
 * @param data Input block (64 bytes)
 * @return None
 *
 * @details Implements SHA-256 compression function per FIPS 180-4.
 *          Processes one 512-bit block and updates the hash state in place.
 */
static void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;

    /* Prepare message schedule (first 16 words are from input) */
    for (int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    }
    /* Extend to 64 words */
    for (int i = 16; i < 64; ++i) {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    /* Initialize working variables */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    /* Main compression loop (64 rounds) */
    for (int i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    /* Add compressed chunk to current hash value */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

/**
 * @brief Computes SHA-256 hash of input data
 * @param data Input data buffer
 * @param len Length of input data in bytes
 * @param hash Output hash buffer (32 bytes)
 * @return None
 *
 * @details Implements complete SHA-256 algorithm per FIPS 180-4.
 *          Includes proper padding and length encoding. Used for password
 *          hash comparison in WebSocket authentication.
 */
void sha256_compute(const uint8_t *data, size_t len, uint8_t hash[32]) {
    /* Initialize hash values (first 32 bits of fractional parts of square roots of first 8 primes)
     */
    uint32_t state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                         0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    uint8_t buffer[64];
    size_t i = 0;

    /* Process complete 64-byte blocks */
    while (i + 64 <= len) {
        memcpy(buffer, data + i, 64);
        sha256_transform(state, buffer);
        i += 64;
    }

    /* Handle final block with padding */
    memset(buffer, 0, 64);
    size_t remaining = len - i;
    memcpy(buffer, data + i, remaining);
    buffer[remaining] = 0x80; /* Append bit '1' */

    /* If not enough room for length, process this block and start a new one */
    if (remaining >= 56) {
        sha256_transform(state, buffer);
        memset(buffer, 0, 64);
    }

    /* Append message length in bits as 64-bit big-endian */
    uint64_t bit_len = len * 8;
    for (int j = 0; j < 8; j++) {
        buffer[63 - j] = (bit_len >> (j * 8)) & 0xFF;
    }
    sha256_transform(state, buffer);

    /* Output hash as big-endian bytes */
    for (int j = 0; j < 8; j++) {
        hash[j * 4] = (state[j] >> 24) & 0xFF;
        hash[j * 4 + 1] = (state[j] >> 16) & 0xFF;
        hash[j * 4 + 2] = (state[j] >> 8) & 0xFF;
        hash[j * 4 + 3] = state[j] & 0xFF;
    }
}

/* ==================== Helper Functions ==================== */

/**
 * @brief Constant-time memory comparison for hash verification
 * @param a First buffer
 * @param b Second buffer
 * @param len Length to compare
 * @return 0 if equal, non-zero if different
 *
 * @details Prevents timing attacks on password hash comparison by ensuring
 *          execution time is independent of where the first mismatch occurs.
 */
static int const_time_memcmp(const uint8_t *a, const uint8_t *b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff;
}

/**
 * @brief Closes WebSocket connection with optional close code
 * @param code Close code (1000 = normal, 4401 = unauthorized)
 * @param reason Optional reason string (max 60 chars)
 *
 * @details Sends WebSocket close frame per RFC 6455, waits briefly for
 *          graceful closure, then closes the underlying TCP socket.
 */
static void console_ws_close(uint16_t code, const char *reason) {
    if (ws_state.sock < 0)
        return;

    /* Build close frame with code and optional reason */
    uint8_t close_payload[64];
    close_payload[0] = (code >> 8) & 0xFF;
    close_payload[1] = code & 0xFF;

    int reason_len = 0;
    if (reason) {
        reason_len = strlen(reason);
        if (reason_len > 60)
            reason_len = 60;
        memcpy(close_payload + 2, reason, reason_len);
    }

    /* Encode and send close frame */
    uint8_t frame[128];
    int frame_len =
        ws_encode_frame(WS_OP_CLOSE, close_payload, 2 + reason_len, frame, sizeof(frame));
    if (frame_len > 0) {
        send(ws_state.sock, frame, frame_len);
        net_beat();
    }

    /* Allow time for graceful closure */
    vTaskDelay(pdMS_TO_TICKS(100));
    disconnect(ws_state.sock);
    closesocket(ws_state.sock);

    ws_state.sock = -1;
    ws_state.active = false;

    INFO_PRINT("%s Connection closed (code %u)\n", CONSOLE_WS_TAG, (unsigned)code);
}

/* ==================== Public Functions ==================== */

/**
 * @brief Initializes console WebSocket subsystem
 * @return true on success, false on failure
 *
 * @details Allocates RX and TX buffers for WebSocket frames.
 *          Call once during NetTask initialization.
 */
bool console_ws_init(void) {
    /* Allocate buffers */
    ws_state.rx_buf = pvPortMalloc(CONSOLE_WS_BUF_SIZE);
    ws_state.tx_buf = pvPortMalloc(CONSOLE_WS_BUF_SIZE);

    if (!ws_state.rx_buf || !ws_state.tx_buf) {
        ERROR_PRINT("%s Failed to allocate buffers\n", CONSOLE_WS_TAG);
        if (ws_state.rx_buf)
            vPortFree(ws_state.rx_buf);
        if (ws_state.tx_buf)
            vPortFree(ws_state.tx_buf);
        return false;
    }

    INFO_PRINT("%s Subsystem initialized\n", CONSOLE_WS_TAG);
    return true;
}

/**
 * @brief Handles WebSocket upgrade request for /ws/console
 * @param sock HTTP socket with upgrade request
 * @param headers Request headers (full HTTP request)
 * @param query Query string (contains pass parameter)
 * @return true if upgrade successful, false if rejected
 *
 * @details Receives plaintext password via ?pass= parameter, hashes it on MCU,
 *          and compares against stored hash. If valid, performs WebSocket handshake.
 */
bool console_ws_handle_upgrade(uint8_t sock, const char *headers, const char *query) {
    INFO_PRINT("%s ========== WebSocket Upgrade Request ==========\r\n", CONSOLE_WS_TAG);
    INFO_PRINT("%s Query string received: '%s'\r\n", CONSOLE_WS_TAG, query ? query : "(NULL)");

    /* Debug: print first 200 chars of headers */
    if (headers) {
        char debug_buf[201];
        strncpy(debug_buf, headers, 200);
        debug_buf[200] = '\0';
        INFO_PRINT("%s Headers (first 200 chars): %s\r\n", CONSOLE_WS_TAG, debug_buf);
    }

    net_beat();

    if (ws_state.active) {
        ERROR_PRINT("%s Connection already active\r\n", CONSOLE_WS_TAG);
        return false;
    }

    /* Extract password from query string */
    char password[64] = {0};
    const char *pass_start = NULL;

    /* Look for "pass=" in query */
    if (query && query[0]) {
        INFO_PRINT("%s Searching for 'pass=' in query...\r\n", CONSOLE_WS_TAG);
        pass_start = strstr(query, "pass=");
        if (pass_start) {
            INFO_PRINT("%s Found 'pass=' at position %d\r\n", CONSOLE_WS_TAG,
                       (int)(pass_start - query));
        } else {
            WARNING_PRINT("%s 'pass=' NOT found in query string\r\n", CONSOLE_WS_TAG);
        }
    } else {
        WARNING_PRINT("%s Query string is NULL or empty\r\n", CONSOLE_WS_TAG);
    }

    /* Fallback: parse from HTTP request line */
    if (!pass_start && headers) {
        INFO_PRINT("%s Trying fallback: parsing from headers...\r\n", CONSOLE_WS_TAG);
        const char *get = strstr(headers, "GET ");
        if (get) {
            const char *line_end = strstr(get, "\r\n");
            if (!line_end) {
                line_end = get + strlen(get);
            }
            const char *uri_start = get + 4; /* skip "GET " */
            const char *space_after_uri = strchr(uri_start, ' ');
            if (space_after_uri && space_after_uri < line_end) {
                const char *qmark = memchr(uri_start, '?', (size_t)(space_after_uri - uri_start));
                if (qmark && (qmark + 1 < space_after_uri)) {
                    const char *qs = qmark + 1;
                    size_t qs_len = (size_t)(space_after_uri - qs);
                    INFO_PRINT("%s Found query in headers: '%.*s'\r\n", CONSOLE_WS_TAG, (int)qs_len,
                               qs);

                    /* Search pass= inside query substring */
                    const char *p = qs;
                    while (p < qs + qs_len) {
                        const char *candidate = strstr(p, "pass=");
                        if (!candidate || candidate >= qs + qs_len) {
                            break;
                        }
                        pass_start = candidate;
                        INFO_PRINT("%s Found 'pass=' in headers at offset %d\r\n", CONSOLE_WS_TAG,
                                   (int)(pass_start - headers));
                        break;
                    }
                }
            }
        }
    }

    if (pass_start) {
        INFO_PRINT("%s Extracting password from 'pass='...\r\n", CONSOLE_WS_TAG);
        pass_start += 5; /* skip "pass=" */
        const char *pass_end = strchr(pass_start, '&');
        if (!pass_end) {
            /* Also stop at space (end of URI) when parsing from request line */
            const char *space = strchr(pass_start, ' ');
            if (space && (!pass_end || space < pass_end)) {
                pass_end = space;
            }
        }

        if (!pass_end) {
            pass_end = pass_start + strlen(pass_start);
        }

        int pass_len = (int)(pass_end - pass_start);
        if (pass_len > 0 && pass_len < (int)sizeof(password)) {
            /* URL decode the password */
            int j = 0;
            for (int i = 0; i < pass_len && j < (int)sizeof(password) - 1; i++) {
                if (pass_start[i] == '%' && i + 2 < pass_len) {
                    char hex[3] = {pass_start[i + 1], pass_start[i + 2], '\0'};
                    password[j++] = (char)strtol(hex, NULL, 16);
                    i += 2;
                } else if (pass_start[i] == '+') {
                    password[j++] = ' ';
                } else {
                    password[j++] = pass_start[i];
                }
            }
            password[j] = '\0';
        }
    }

    INFO_PRINT("%s Password extracted: '%s' (length=%d)\r\n", CONSOLE_WS_TAG,
               password[0] ? password : "(EMPTY)", (int)strlen(password));
    net_beat();

    if (!password[0]) {
        WARNING_PRINT("%s Empty or missing password in WebSocket request\r\n", CONSOLE_WS_TAG);
        return false;
    }

    /* Hash the password */
    INFO_PRINT("%s Computing SHA-256 hash of password...\r\n", CONSOLE_WS_TAG);
    uint8_t client_hash[32];
    sha256_compute((const uint8_t *)password, strlen(password), client_hash);

    INFO_PRINT("%s Client hash: ", CONSOLE_WS_TAG);
    for (int i = 0; i < 32; i++) {
        INFO_PRINT("%02x", client_hash[i]);
    }
    INFO_PRINT("\r\n");
    net_beat();

    /* Load stored hash from EEPROM */
    INFO_PRINT("%s Reading stored hash from EEPROM...\r\n", CONSOLE_WS_TAG);
    uint8_t stored_hash[32];
    if (!EEPROM_ReadConsoleHash(stored_hash)) {
        ERROR_PRINT("%s Failed to read stored hash\r\n", CONSOLE_WS_TAG);
        return false;
    }

    INFO_PRINT("%s Stored hash: ", CONSOLE_WS_TAG);
    for (int i = 0; i < 32; i++) {
        INFO_PRINT("%02x", stored_hash[i]);
    }
    INFO_PRINT("\r\n");
    net_beat();

    /* Compare hashes */
    INFO_PRINT("%s Comparing hashes...\r\n", CONSOLE_WS_TAG);
    if (const_time_memcmp(client_hash, stored_hash, 32) != 0) {
        WARNING_PRINT("%s Authentication FAILED - hash mismatch\r\n", CONSOLE_WS_TAG);
        return false;
    }

    INFO_PRINT("%s Authentication SUCCESSFUL!\r\n", CONSOLE_WS_TAG);
    net_beat();

    /* Extract Sec-WebSocket-Key from headers */
    char ws_key[32] = {0};
    const char *key_hdr = strstr(headers, "Sec-WebSocket-Key:");
    if (key_hdr) {
        key_hdr += 18; /* skip header name */
        while (*key_hdr == ' ')
            key_hdr++;
        const char *key_end = strstr(key_hdr, "\r\n");
        if (key_end) {
            int len = key_end - key_hdr;
            if (len > 0 && len < (int)sizeof(ws_key)) {
                memcpy(ws_key, key_hdr, len);
                ws_key[len] = '\0';
            }
        }
    }
    net_beat();

    if (!ws_key[0]) {
        ERROR_PRINT("%s Missing Sec-WebSocket-Key\n", CONSOLE_WS_TAG);
        return false;
    }

    /* Generate accept key (SHA-1 of key + GUID, then base64) */
    char accept_key[32];
    if (!ws_generate_accept_key(ws_key, accept_key)) {
        ERROR_PRINT("%s Failed to generate accept key\n", CONSOLE_WS_TAG);
        return false;
    }
    net_beat();

    /* Send WebSocket handshake response */
    char response[256];
    int len = snprintf(response, sizeof(response),
                       "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: %s\r\n"
                       "\r\n",
                       accept_key);

    if (send(sock, (uint8_t *)response, len) != len) {
        ERROR_PRINT("%s Failed to send handshake\n", CONSOLE_WS_TAG);
        return false;
    }
    net_beat();

    /* Mark as active and take ownership of socket */
    ws_state.sock = sock;
    ws_state.active = true;
    ws_state.last_activity_ms = to_ms_since_boot(get_absolute_time());

    INFO_PRINT("%s WebSocket connection established on socket %d\n", CONSOLE_WS_TAG, sock);

    /* Send welcome message */
    const char *welcome = "\r\nENERGIS PDU Console\r\nType 'help' for commands\r\n\r\n";
    console_ws_send_text(welcome, strlen(welcome));

    return true;
}

/**
 * @brief Processes console WebSocket events (call from NetTask loop)
 * @return None
 *
 * @details Handles incoming WebSocket frames, forwards to console,
 *          sends console output to client, implements timeout.
 *          Should be called regularly (e.g., every 10ms) from NetTask.
 */
void console_ws_process(void) {
    if (!ws_state.active || ws_state.sock < 0)
        return;

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    /* Check timeout (5 minutes default) */
    if ((now_ms - ws_state.last_activity_ms) > CONSOLE_WS_TIMEOUT_MS) {
        WARNING_PRINT("%s Timeout, closing connection\n", CONSOLE_WS_TAG);
        console_ws_close(1000, "Timeout");
        return;
    }

    /* Check socket status */
    uint8_t status = getSn_SR(ws_state.sock);
    if (status != SOCK_ESTABLISHED) {
        INFO_PRINT("%s Socket not established (status=0x%02X)\n", CONSOLE_WS_TAG, status);
        ws_state.active = false;
        ws_state.sock = -1;
        return;
    }
    net_beat();

    /* Receive data from WebSocket */
    int recv_len = recv(ws_state.sock, ws_state.rx_buf, CONSOLE_WS_BUF_SIZE);
    if (recv_len > 0) {
        ws_state.last_activity_ms = now_ms;

        /* Parse WebSocket frame */
        ws_frame_t frame;
        int consumed = ws_parse_frame(ws_state.rx_buf, recv_len, &frame);

        if (consumed > 0) {
            /* Unmask payload (client-to-server frames are always masked) */
            if (frame.masked) {
                ws_unmask_payload(frame.payload, frame.payload_len, frame.mask);
            }

            /* Handle different frame types */
            switch (frame.opcode) {
            case WS_OP_TEXT:
                /* Forward text input to console task */
                if (frame.payload && frame.payload_len > 0) {
                    Console_SendInput((char *)frame.payload, (int)frame.payload_len);
                }
                break;

            case WS_OP_CLOSE:
                /* Client requested close */
                INFO_PRINT("%s Client requested close\n", CONSOLE_WS_TAG);
                console_ws_close(1000, "");
                return;

            case WS_OP_PING:
                /* Respond with pong */
                {
                    int pong_len =
                        ws_encode_frame(WS_OP_PONG, frame.payload, (uint16_t)frame.payload_len,
                                        ws_state.tx_buf, CONSOLE_WS_BUF_SIZE);
                    if (pong_len > 0) {
                        send(ws_state.sock, ws_state.tx_buf, pong_len);
                    }
                }
                break;

            case WS_OP_PONG:
                /* Ignore (response to our ping) */
                break;

            default:
                WARNING_PRINT("%s Unknown opcode 0x%02X\n", CONSOLE_WS_TAG, frame.opcode);
                break;
            }
        }
        net_beat();
    }

    /* Forward console output to WebSocket */
    char console_out[256];
    int out_len = Console_GetOutput(console_out, sizeof(console_out) - 1);
    if (out_len > 0) {
        console_out[out_len] = '\0';
        console_ws_send_text(console_out, out_len);
    }
}

/**
 * @brief Checks if console WebSocket is active
 * @return true if client connected, false otherwise
 */
bool console_ws_is_active(void) { return ws_state.active && ws_state.sock >= 0; }

/**
 * @brief Sends text to console WebSocket client
 * @param text Text string to send
 * @param len Text length
 * @return true if sent, false if no client or send failed
 *
 * @details Encodes text into WebSocket frame and sends to client.
 *          Updates activity timestamp on successful send.
 */
bool console_ws_send_text(const char *text, int len) {
    if (!ws_state.active || ws_state.sock < 0 || !text || len <= 0)
        return false;

    /* Encode frame */
    int frame_len = ws_encode_frame(WS_OP_TEXT, (const uint8_t *)text, (uint16_t)len,
                                    ws_state.tx_buf, CONSOLE_WS_BUF_SIZE);
    if (frame_len <= 0)
        return false;

    /* Send */
    int sent = send(ws_state.sock, ws_state.tx_buf, frame_len);
    if (sent != frame_len) {
        ERROR_PRINT("%s Send failed\n", CONSOLE_WS_TAG);
        return false;
    }

    ws_state.last_activity_ms = to_ms_since_boot(get_absolute_time());
    net_beat();
    return true;
}