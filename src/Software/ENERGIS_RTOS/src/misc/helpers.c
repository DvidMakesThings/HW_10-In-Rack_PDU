/**
 * @file helpers.c
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup misc_helpers01 1. Helper Functions
 * @ingroup misc_helpers
 * @brief General-purpose helper functions for string processing and utilities
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-07
 * @details Provides utility functions for URL decoding, form parsing, and
 *          other common string operations used throughout the application.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/**
 * @brief Converts a hexadecimal character to its integer value
 * @param c The hexadecimal character ('0'-'9', 'A'-'F', 'a'-'f')
 * @return The integer value (0-15), or -1 if invalid
 * @note Helper function for URL decoding
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
 * @brief Extract key=value from URL-encoded body (in-place), returns NULL if missing
 * @param body The URL-encoded body string to search
 * @param key The key to search for
 * @return Pointer to static buffer containing the value, or NULL if not found
 * @note The returned pointer is to a static buffer that will be overwritten
 *       on the next call. The value is not URL-decoded by this function.
 */
char *get_form_value(const char *body, const char *key) {
    static char value[64];
    char search[32];

    /* Build search string "key=" */
    snprintf(search, sizeof(search), "%s=", key);

    /* Find the key in the body */
    char *start = strstr(body, search);
    if (!start)
        return NULL;

    /* Move past "key=" */
    start += strlen(search);

    /* Find end of value (next '&' or end of string) */
    char *end = strstr(start, "&");
    if (!end)
        end = start + strlen(start);

    /* Extract value into static buffer */
    size_t len = end - start;
    if (len >= sizeof(value))
        len = sizeof(value) - 1;

    memcpy(value, start, len);
    value[len] = '\0';

    return value;
}

/**
 * @brief Decode '+' to ' ' and "%XX" to char, in-place
 * @param s The string to decode (modified in-place)
 * @return None
 * @note This function modifies the input string in-place, converting URL
 *       encoding to regular characters. '+' becomes space, and %XX sequences
 *       are converted to their corresponding characters.
 */
void urldecode(char *s) {
    char *dst = s;
    char *src = s;

    while (*src) {
        if (*src == '+') {
            /* Convert '+' to space */
            *dst++ = ' ';
            src++;
        } else if (src[0] == '%' && hexval(src[1]) >= 0 && hexval(src[2]) >= 0) {
            /* Convert %XX to character */
            int hi = hexval(src[1]);
            int lo = hexval(src[2]);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        } else {
            /* Copy character as-is */
            *dst++ = *src++;
        }
    }

    /* Null-terminate the result */
    *dst = '\0';
}

/**
 * @brief Read voltage from the selected ADC channel with averaging.
 *
 * @param ch ADC channel index compatible with adc_select_input
 * @param len Unused, present for legacy signature compatibility
 * @return Measured voltage as float
 */
float get_Voltage(uint8_t ch) {
    adc_set_clkdiv(96.0f); /* slow down ADC clock */
    adc_select_input(ch);
    (void)adc_read();             /* throwaway sample */
    vTaskDelay(pdMS_TO_TICKS(1)); /* allow S&H to settle ~0.1 ms rounded up */

    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) {
        acc += adc_read();
    }

    float vtap = (acc / 16.0f) * (ADC_VREF / ADC_MAX);
    return vtap * 1 /*ADC_TOL*/; /* apply +0.5 percent correction */
}

/******************************************************************************
 *                     PUBLIC HELPER ADDED FOR NETTASK                        *
 ******************************************************************************/

/**
 * @brief Apply StorageTask networkInfo into W5500 and initialize the chip.
 *
 * @param ni Pointer to networkInfo (ip, gw, sn, dns, mac, dhcp mode)
 * @return true on success, false otherwise
 *
 * @details
 * This function maps StorageTask schema to the driver config struct,
 * calls w5500_set_network and then w5500_chip_init. It allows NetTask
 * to configure networking without needing to know the internal type.
 */
bool ethernet_apply_network_from_storage(const networkInfo *ni) {
    if (!ni)
        return false;

    w5500_NetConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Copy addressing */
    for (int i = 0; i < 4; ++i) {
        cfg.ip[i] = ni->ip[i];
        cfg.gw[i] = ni->gw[i];
        cfg.sn[i] = ni->sn[i];
        cfg.dns[i] = ni->dns[i];
    }
    for (int i = 0; i < 6; ++i) {
        cfg.mac[i] = ni->mac[i];
    }

    /* DHCP or static mode mapping */
    cfg.dhcp = (ni->dhcp == EEPROM_NETINFO_DHCP) ? 1 : 0;

    /* Apply into driver */
    w5500_set_network(&cfg);

    /* Final chip init with this config */
    if (!w5500_chip_init(&cfg)) {
        return false;
    }

    /* Optional: print applied configuration */
    w5500_print_network(&cfg);
    setNetworkLink(true);
    return true;
}

/**
 * @}
 */