/**
 * @file src/misc/helpers.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * Implementation of system-wide utility functions including URL parsing, network
 * configuration bridging, and early boot diagnostics. This module provides helper
 * functions that support multiple subsystems without belonging to a specific domain.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

#define HELPERS_TAG "[HELPERS]"

/** @brief Magic tag to validate retained snapshot content. */
#define HELPERS_SNAPSHOT_MAGIC (0x534E4150u) /* 'SNAP' */

/** @brief Snapshot format version. Increase if layout changes. */
#define HELPERS_SNAPSHOT_VERSION (0x0001u)

/**
 * @brief Retained early-boot snapshot persisted in noinit RAM.
 *
 * The structure is intentionally simple and self-contained so it can be written
 * before any subsystem is up. It captures information that is useful to diagnose
 * watchdog bites and reset causes without relying on the logger at that time.
 */
typedef struct {
    uint32_t magic;          /**< Magic tag set to HELPERS_SNAPSHOT_MAGIC when valid. */
    uint16_t version;        /**< Structure version set to HELPERS_SNAPSHOT_VERSION. */
    uint16_t reserved;       /**< Reserved for alignment and future use. */
    uint32_t boots;          /**< Monotonic boot counter since first initialization. */
    uint32_t reset_raw_bits; /**< Platform specific raw reset reason bits at entry. */
    uint32_t wd_scratch[8];  /**< Copy of watchdog scratch[0..7] at entry. */
} helpers_boot_snapshot_t;

/* ================================ Storage ================================== */

/**
 * @brief Retained snapshot buffer in non-initialized memory.
 *
 * Placed in a noinit section so its content survives across soft resets.
 * On first power-on it will contain random data until initialized by
 * Helpers_EarlyBootSnapshot().
 *
 * @ingroup misc_helpers
 */
#if defined(__GNUC__)
__attribute__((section(".noinit")))
#endif
static helpers_boot_snapshot_t s_bootsnap;

/**
 * @brief Convert hexadecimal character to integer value.
 *
 * Converts a single hex digit character to its numeric value. Supports
 * both uppercase and lowercase hex digits.
 *
 * @param c Hexadecimal character ('0'-'9', 'A'-'F', 'a'-'f')
 *
 * @return Integer value 0-15 for valid hex digits, -1 for invalid characters
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
    static char value[128];
    char search[32];

    /* Build search pattern "key=" */
    snprintf(search, sizeof(search), "%s=", key);

    /* Locate key in body string */
    char *start = strstr(body, search);
    if (!start) {
        return NULL;
    }

    /* Advance past "key=" prefix */
    start += strlen(search);

    /* Find value terminator (next '&' or end of string) */
    char *end = strstr(start, "&");
    if (!end)
        end = start + strlen(start);

    /* Copy value to static buffer with bounds checking */
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
            /* Plus sign decodes to space */
            *dst++ = ' ';
            src++;
        } else if (src[0] == '%' && hexval(src[1]) >= 0 && hexval(src[2]) >= 0) {
            /* Percent-encoded hex sequence: decode to byte */
            int hi = hexval(src[1]);
            int lo = hexval(src[2]);
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        } else {
            /* Regular character: copy verbatim */
            *dst++ = *src++;
        }
    }

    /* Ensure null termination */
    *dst = '\0';
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
    /* Validate input pointer */
    if (!ni) {
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_LOGGER, ERR_SEV_ERROR, ERR_FID_HELPERS, 0x1);
        ERROR_PRINT_CODE(errorcode, "%s Null networkInfo pointer\r\n", HELPERS_TAG);
        Storage_EnqueueErrorCode(errorcode);
#endif
        return false;
    }

    /* Prepare driver configuration structure */
    w5500_NetConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    /* Copy IP addressing parameters */
    for (int i = 0; i < 4; ++i) {
        cfg.ip[i] = ni->ip[i];
        cfg.gw[i] = ni->gw[i];
        cfg.sn[i] = ni->sn[i];
        cfg.dns[i] = ni->dns[i];
    }

    /* Copy MAC address */
    for (int i = 0; i < 6; ++i) {
        cfg.mac[i] = ni->mac[i];
    }

    /* Map DHCP mode */
    cfg.dhcp = (ni->dhcp == EEPROM_NETINFO_DHCP) ? 1 : 0;

    /* Apply configuration to W5500 driver */
    w5500_set_network(&cfg);

    /* Initialize chip with new configuration */
    if (!w5500_chip_init(&cfg)) {
        return false;
    }

    /* Flash Ethernet LED to indicate successful configuration */
    Switch_SetEthLed(true, 10);
    return true;
}

/**
 * @brief Print detailed reset cause diagnostics with hardware register decode.
 *
 * Reads RP2040/RP2350 reset cause hardware registers and outputs comprehensive
 * diagnostics. If fault breadcrumbs are present (0xBEEF signature in scratch[0]),
 * decodes and reports the fault type and context.
 *
 * @return None
 */
void Boot_LogResetCause(void) {
    uint32_t chip = vreg_and_chip_reset_hw->chip_reset;
    uint32_t wd = watchdog_hw->reason;

    const uint32_t sdk_magic = watchdog_hw->scratch[4]; /* SDK marker (WATCHDOG_NON_REBOOT_MAGIC) */

    const unsigned had_por = (chip >> 0) & 1u;
    const unsigned had_run = (chip >> 1) & 1u;
    const unsigned had_wd = (chip >> 2) & 1u;
    const unsigned had_vreg = (chip >> 3) & 1u;
    const unsigned had_temp = (chip >> 4) & 1u;

    /* Print reset flags summary */
    INFO_PRINT("[BOOT] cause: chip=0x%08lx had{por=%u,run=%u,wd=%u,vreg=%u,temp=%u} "
               "wd_reason=0x%08lx\r\n\r\n",
               (unsigned long)chip, had_por, had_run, had_wd, had_vreg, had_temp,
               (unsigned long)wd);

    /* Dump all watchdog scratch registers for diagnostic correlation */
    {
        INFO_PRINT("[BOOT] Watchdog Scratch Register dump:\r\n");
        INFO_PRINT("[BOOT] wd_scratch0=0x%08lx\r\n", (unsigned long)watchdog_hw->scratch[0]);
        INFO_PRINT("[BOOT] wd_scratch1=0x%08lx\r\n", (unsigned long)watchdog_hw->scratch[1]);
        INFO_PRINT("[BOOT] wd_scratch2=0x%08lx\r\n", (unsigned long)watchdog_hw->scratch[2]);
        INFO_PRINT("[BOOT] wd_scratch3=0x%08lx\r\n", (unsigned long)watchdog_hw->scratch[3]);
        INFO_PRINT("[BOOT] wd_scratch4=0x%08lx\r\n", (unsigned long)watchdog_hw->scratch[4]);
        INFO_PRINT("[BOOT] wd_scratch5=0x%08lx\r\n", (unsigned long)watchdog_hw->scratch[5]);
        INFO_PRINT("[BOOT] wd_scratch6=0x%08lx\r\n", (unsigned long)watchdog_hw->scratch[6]);
        INFO_PRINT("[BOOT] wd_scratch7=0x%08lx\r\n\r\n", (unsigned long)watchdog_hw->scratch[7]);
    }

    /* Check for fault breadcrumb signature in scratch[0] */
    uint32_t s0 = watchdog_hw->scratch[0];
    if ((s0 & 0xFFFF0000u) == 0xBEEF0000u) {
        uint32_t cause = s0 & 0x0000FFFFu;
        uint32_t s1 = watchdog_hw->scratch[1];
        uint32_t s2 = watchdog_hw->scratch[2];
        uint32_t s3 = watchdog_hw->scratch[3];
        uint32_t s4 = watchdog_hw->scratch[4];
        uint32_t s5 = watchdog_hw->scratch[5];
        uint32_t s6 = watchdog_hw->scratch[6];
        uint32_t s7 = watchdog_hw->scratch[7];

        /* Decode fault cause from lower 16 bits */
        const char *cause_str = "UNKNOWN";
        switch (cause) {
        case 0xF1:
            cause_str = "HardFault";
            break;
        case 0xF2:
            cause_str = "StackOverflow";
            break;
        case 0xF3:
            cause_str = "MallocFail";
            break;
        case 0xF4:
            cause_str = "Assert";
            break;
        default:
            break;
        }
#if ERRORLOGGER
        uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_LOGGER, ERR_SEV_ERROR, ERR_FID_HELPERS, 0x2);
        ERROR_PRINT_CODE(
            errorcode,
            "%s last fault: cause=%s(0x%04lx) lr=0x%08lx a=0x%08lx b=0x%08lx c=0x%08lx "
            "d=0x%08lx e=0x%08lx f=0x%08lx\r\n\r\n",
            HELPERS_TAG, cause_str, (unsigned long)cause, (unsigned long)s1, (unsigned long)s2,
            (unsigned long)s3, (unsigned long)s4, (unsigned long)s5, (unsigned long)s6,
            (unsigned long)s7);
        Storage_EnqueueErrorCode(errorcode);
#endif

        /* Clear fault signature to prevent stale reporting on subsequent boot */
        watchdog_hw->scratch[0] = 0u;
    }
}

/* =============================== Utilities ================================= */

/**
 * @brief Read platform reset reason bits from hardware.
 *
 * Platform-specific function to read raw reset cause bits. For RP2040/RP2350,
 * reads the watchdog reason register.
 *
 * @return Raw reset bits, platform-specific encoding
 */
static inline uint32_t helpers_read_reset_bits(void) {
    uint32_t bits = 0u;
#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(PICO_PLATFORM)
    /* On RP2040/2350, watchdog_get_reason() returns the hardware reset reason bits. */
    bits = watchdog_hw->reason;
#endif
    return bits;
}

/**
 * @brief Copy watchdog scratch registers to destination buffer.
 *
 * Reads all 8 watchdog scratch registers into the provided array.
 * For non-RP platforms, fills the buffer with zeros.
 *
 * @param dst Destination array of 8 uint32_t values
 *
 * @return None
 */
static inline void helpers_copy_wd_scratch(uint32_t dst[8]) {
#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(PICO_PLATFORM)
    /* Directly snapshot the hardware watchdog scratch registers. */
    dst[0] = watchdog_hw->scratch[0];
    dst[1] = watchdog_hw->scratch[1];
    dst[2] = watchdog_hw->scratch[2];
    dst[3] = watchdog_hw->scratch[3];
    dst[4] = watchdog_hw->scratch[4];
    dst[5] = watchdog_hw->scratch[5];
    dst[6] = watchdog_hw->scratch[6];
    dst[7] = watchdog_hw->scratch[7];
#else
    /* Fallback for non RP platforms. */
    memset(dst, 0, 8u * sizeof(uint32_t));
#endif
}

/**
 * @brief Clear boot snapshot structure to zero.
 *
 * @param s Pointer to snapshot structure
 *
 * @return None
 */
static inline void helpers_clear_snapshot(helpers_boot_snapshot_t *s) { memset(s, 0, sizeof(*s)); }

/* ================================ DEBUG API ====================================== */

void Helpers_EarlyBootSnapshot(void) {
    /* Validate or initialize snapshot structure */
    if (s_bootsnap.magic != HELPERS_SNAPSHOT_MAGIC ||
        s_bootsnap.version != HELPERS_SNAPSHOT_VERSION) {
        helpers_clear_snapshot(&s_bootsnap);
        s_bootsnap.magic = HELPERS_SNAPSHOT_MAGIC;
        s_bootsnap.version = HELPERS_SNAPSHOT_VERSION;
        s_bootsnap.boots = 0u;
    }

    /* Increment monotonic boot counter */
    s_bootsnap.boots += 1u;

    /* Capture reset diagnostics before any initialization */
    s_bootsnap.reset_raw_bits = helpers_read_reset_bits();
    helpers_copy_wd_scratch(s_bootsnap.wd_scratch);
}

void Helpers_LateBootDumpAndClear(void) {
    /* Check for valid snapshot */
    if (s_bootsnap.magic != HELPERS_SNAPSHOT_MAGIC ||
        s_bootsnap.version != HELPERS_SNAPSHOT_VERSION) {
        DEBUG_PRINT("[INFO] ========================================\r\n");
        DEBUG_PRINT("[INFO] [InitTask] Early snapshot: <none>\r\n");
        DEBUG_PRINT("[INFO] ========================================\r\n");
        return;
    }

    /* Output snapshot diagnostics */
    DEBUG_PRINT("[INFO] ========================================\r\n");
    DEBUG_PRINT("[INFO] [InitTask] Early snapshot dump:\r\n");
    DEBUG_PRINT("[INFO]   boots=%lu raw=0x%08lX\r\n", (unsigned long)s_bootsnap.boots,
                (unsigned long)s_bootsnap.reset_raw_bits);

    DEBUG_PRINT("[INFO]   wd_scratch[0..7]:\r\n");
    DEBUG_PRINT("[INFO]     0=0x%08lX 1=0x%08lX 2=0x%08lX 3=0x%08lX\r\n",
                (unsigned long)s_bootsnap.wd_scratch[0], (unsigned long)s_bootsnap.wd_scratch[1],
                (unsigned long)s_bootsnap.wd_scratch[2], (unsigned long)s_bootsnap.wd_scratch[3]);
    DEBUG_PRINT("[INFO]     4=0x%08lX 5=0x%08lX 6=0x%08lX 7=0x%08lX\r\n",
                (unsigned long)s_bootsnap.wd_scratch[4], (unsigned long)s_bootsnap.wd_scratch[5],
                (unsigned long)s_bootsnap.wd_scratch[6], (unsigned long)s_bootsnap.wd_scratch[7]);

    DEBUG_PRINT("[INFO] ========================================\r\n");

    /* Clear transient fields while preserving boot counter */
    s_bootsnap.reset_raw_bits = 0u;
    for (unsigned i = 0; i < 8; ++i) {
        s_bootsnap.wd_scratch[i] = 0u;
    }
}

/**
 * @}
 */