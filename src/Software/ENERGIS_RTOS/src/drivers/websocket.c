/**
 * @file src/web_handlers/websocket_protocol.c
 * @author DvidMakesThings - David Sipos
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

#include "../CONFIG.h"

static inline void net_beat(void) { Health_Heartbeat(HEALTH_ID_NET); }

/* ==================== Base64 Encoding ==================== */

/**
 * @brief Base64 encoding table
 */
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/**
 * @brief Encodes binary data to Base64 string
 * @param input Input binary data
 * @param len Length of input data
 * @param output Output buffer (must be at least ((len+2)/3)*4 + 1 bytes)
 * @return None
 *
 * @details Implements standard Base64 encoding per RFC 4648. Handles padding
 *          automatically with '=' characters when input length is not divisible by 3.
 */
static void base64_encode(const uint8_t *input, size_t len, char *output) {
    size_t i = 0, j = 0;

    while (i < len) {
        uint32_t a = i < len ? input[i++] : 0;
        uint32_t b = i < len ? input[i++] : 0;
        uint32_t c = i < len ? input[i++] : 0;
        uint32_t triple = (a << 16) + (b << 8) + c;

        output[j++] = base64_table[(triple >> 18) & 0x3F];
        output[j++] = base64_table[(triple >> 12) & 0x3F];
        output[j++] = base64_table[(triple >> 6) & 0x3F];
        output[j++] = base64_table[triple & 0x3F];
    }

    size_t mod = len % 3;
    if (mod == 1) {
        output[j - 2] = '=';
        output[j - 1] = '=';
    } else if (mod == 2) {
        output[j - 1] = '=';
    }

    output[j] = '\0';
}

/* ==================== SHA-1 Implementation ==================== */

/**
 * @brief Rotate left operation for SHA-1
 */
#define ROL(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/**
 * @brief SHA-1 transformation function (processes one 64-byte block)
 * @param state Current hash state (5 x 32-bit words)
 * @param buffer Input block (64 bytes)
 * @return None
 *
 * @details Implements SHA-1 compression function per RFC 3174. Processes one
 *          512-bit block and updates the hash state in place.
 */
static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t w[80];

    /* Prepare message schedule */
    for (int i = 0; i < 16; i++) {
        w[i] = (buffer[i * 4] << 24) | (buffer[i * 4 + 1] << 16) | (buffer[i * 4 + 2] << 8) |
               buffer[i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) {
        w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

    /* Main loop with four different logical functions */
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        uint32_t temp = ROL(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROL(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

/**
 * @brief Computes SHA-1 hash of input data
 * @param data Input data buffer
 * @param len Length of input data in bytes
 * @param hash Output hash buffer (20 bytes)
 * @return None
 *
 * @details Implements complete SHA-1 algorithm including padding and length encoding.
 *          Used for WebSocket handshake accept key generation per RFC 6455.
 */
static void sha1_compute(const uint8_t *data, size_t len, uint8_t hash[20]) {
    uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint8_t buffer[64];
    size_t i = 0;

    /* Process complete 64-byte blocks */
    while (i + 64 <= len) {
        memcpy(buffer, data + i, 64);
        sha1_transform(state, buffer);
        i += 64;
    }

    /* Handle final block with padding */
    memset(buffer, 0, 64);
    size_t remaining = len - i;
    memcpy(buffer, data + i, remaining);
    buffer[remaining] = 0x80;

    if (remaining >= 56) {
        sha1_transform(state, buffer);
        memset(buffer, 0, 64);
    }

    /* Append message length in bits as 64-bit big-endian */
    uint64_t bit_len = len * 8;
    for (int j = 0; j < 8; j++) {
        buffer[63 - j] = (bit_len >> (j * 8)) & 0xFF;
    }
    sha1_transform(state, buffer);

    /* Output hash as big-endian bytes */
    for (int j = 0; j < 5; j++) {
        hash[j * 4] = (state[j] >> 24) & 0xFF;
        hash[j * 4 + 1] = (state[j] >> 16) & 0xFF;
        hash[j * 4 + 2] = (state[j] >> 8) & 0xFF;
        hash[j * 4 + 3] = state[j] & 0xFF;
    }
}

/* ==================== WebSocket Functions ==================== */

/**
 * @brief Generates WebSocket accept key from client key
 * @param client_key Client's Sec-WebSocket-Key header value (24 chars base64)
 * @param accept_key Output buffer (min 29 bytes for base64 + null)
 * @return true on success, false on failure
 *
 * @details Per RFC 6455: SHA1(client_key + GUID) then base64 encode.
 *          The GUID is the fixed string "258EAFA5-E914-47DA-95CA-C5AB0DC85B11".
 *          This proves to the client that the server understands WebSocket protocol.
 */
bool ws_generate_accept_key(const char *client_key, char *accept_key) {
    if (!client_key || !accept_key)
        return false;

    /* WebSocket GUID from RFC 6455 */
    static const char *ws_guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    /* Concatenate client key + GUID */
    char concat[128];
    int n = snprintf(concat, sizeof(concat), "%s%s", client_key, ws_guid);
    if (n < 0 || n >= (int)sizeof(concat))
        return false;
    net_beat();

    /* SHA-1 hash */
    uint8_t hash[20];
    sha1_compute((const uint8_t *)concat, (size_t)n, hash);
    net_beat();

    /* Base64 encode */
    base64_encode(hash, 20, accept_key);
    return true;
}

/**
 * @brief Parses WebSocket frame from buffer
 * @param buf Input buffer containing frame data
 * @param len Buffer length
 * @param frame Output frame structure (payload points into buf)
 * @return Number of bytes consumed, or -1 on error, 0 if incomplete
 *
 * @details Supports frames up to 64KB payload. Does not allocate memory.
 *          Caller must ensure buf remains valid while using frame->payload.
 *          Handles masked and unmasked frames per RFC 6455.
 */
int ws_parse_frame(const uint8_t *buf, int len, ws_frame_t *frame) {
    if (!buf || !frame || len < 2)
        return 0; /* need at least 2 bytes */

    frame->fin = (buf[0] & 0x80) != 0;
    frame->opcode = buf[0] & 0x0F;
    frame->masked = (buf[1] & 0x80) != 0;

    uint64_t payload_len = buf[1] & 0x7F;
    int hdr_len = 2;

    /* Extended payload length */
    if (payload_len == 126) {
        if (len < 4)
            return 0;
        payload_len = ((uint64_t)buf[2] << 8) | buf[3];
        hdr_len = 4;
    } else if (payload_len == 127) {
        /* 64-bit length not supported for embedded */
        return -1;
    }
    net_beat();

    /* Mask key */
    if (frame->masked) {
        if (len < hdr_len + 4)
            return 0;
        memcpy(frame->mask, buf + hdr_len, 4);
        hdr_len += 4;
    }

    /* Check if we have full payload */
    if (len < hdr_len + (int)payload_len)
        return 0;

    frame->payload_len = payload_len;
    frame->payload = (uint8_t *)(buf + hdr_len);

    return hdr_len + (int)payload_len;
}

/**
 * @brief Encodes WebSocket frame to buffer (server -> client, never masked)
 * @param opcode Frame opcode (text, binary, close, ping, pong)
 * @param payload Payload data
 * @param payload_len Payload length
 * @param buf Output buffer
 * @param buflen Buffer size
 * @return Number of bytes written, or -1 on error
 *
 * @details Creates properly formatted WebSocket frame per RFC 6455.
 *          Server-to-client frames are never masked (mask bit always 0).
 *          Supports payload lengths up to 65535 bytes using extended length encoding.
 */
int ws_encode_frame(uint8_t opcode, const uint8_t *payload, uint16_t payload_len, uint8_t *buf,
                    int buflen) {
    if (!buf || buflen < 2)
        return -1;

    int hdr_len = 2;

    /* Byte 0: FIN + opcode */
    buf[0] = 0x80 | (opcode & 0x0F);

    /* Byte 1: MASK=0 + payload len */
    if (payload_len < 126) {
        buf[1] = (uint8_t)payload_len;
    } else {
        if (buflen < 4)
            return -1;
        buf[1] = 126;
        buf[2] = (uint8_t)(payload_len >> 8);
        buf[3] = (uint8_t)(payload_len & 0xFF);
        hdr_len = 4;
    }
    net_beat();

    /* Check space for payload */
    if (buflen < hdr_len + payload_len)
        return -1;

    /* Copy payload */
    if (payload && payload_len > 0)
        memcpy(buf + hdr_len, payload, payload_len);

    return hdr_len + payload_len;
}

/**
 * @brief Unmasks WebSocket payload in-place
 * @param payload Payload buffer
 * @param len Payload length
 * @param mask Mask key (4 bytes)
 *
 * @details Applies XOR mask to payload per RFC 6455.
 *          Client-to-server frames must always be masked.
 *          The mask is applied byte-by-byte in a repeating 4-byte pattern.
 */
void ws_unmask_payload(uint8_t *payload, uint64_t len, const uint8_t mask[4]) {
    for (uint64_t i = 0; i < len; i++) {
        payload[i] ^= mask[i & 3];
    }
}