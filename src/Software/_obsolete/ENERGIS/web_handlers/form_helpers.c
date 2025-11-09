/**
 * @file form_helpers.c
 * @author DvidMakesThings - David Sipos
 * @defgroup webui2 2. Form Helpers
 * @ingroup webui
 * @brief Helper functions for handling form data in HTTP requests.
 * @{
 * @version 1.0
 * @date 2025-05-23
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "form_helpers.h"
#include "CONFIG.h"

/**
 * @brief Converts a hexadecimal character to its integer value.
 * @param c The hexadecimal character.
 * @return The integer value of the character, or -1 if invalid.
 */
static inline int hexval(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/**
 * @brief Extract key=value from URL‐encoded body (in‐place), returns NULL if missing.
 * @param c The hexadecimal character.
 * @return The integer value of the character, or -1 if invalid.
 */
char *get_form_value(const char *body, const char *key) {
    static char value[64];
    char search[32];
    snprintf(search, sizeof(search), "%s=", key);
    char *start = strstr(body, search);
    if (!start)
        return NULL;
    start += strlen(search);
    char *end = strstr(start, "&");
    if (!end)
        end = start + strlen(start);
    size_t len = end - start;
    if (len >= sizeof(value))
        len = sizeof(value) - 1;
    memcpy(value, start, len);
    value[len] = '\0';
    return value;
}

/**
 * @brief Decode '+'→' ' and "%XX"→char, in‐place.
 * @param c The hexadecimal character.
 * @return None
 */
void urldecode(char *s) {
    char *dst = s, *src = s;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (src[0] == '%' && hexval(src[1]) >= 0 && hexval(src[2]) >= 0) {
            int hi = hexval(src[1]), lo = hexval(src[2]);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}
/**
 * @}
 */