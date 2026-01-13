/**
 * @file src/misc/crashlog.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0.0
 * @date 2025-11-06
 *
 * @details
 * Implementation of crash logging system with retained RAM storage and CRC integrity.
 * This module maintains a crash log structure in non-initialized RAM section that
 * survives across resets, enabling post-mortem diagnostics of fault conditions.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* Log through the non-blocking logger to avoid CDC blocking. */
#ifndef CRASH_LOG
#ifdef ERROR_PRINT
#define CRASH_LOG(...) ERROR_PRINT(__VA_ARGS__)
#else
#define CRASH_LOG(...)                                                                             \
    do {                                                                                           \
    } while (0)
#endif
#endif

/* Retained region */
#if defined(__GNUC__)
__attribute__((section(".noinit"))) static crashlog_t s_crash;
#else
static crashlog_t s_crash;
#endif

/* Header constants */
#define CRASH_HDR_MAGIC 0x43524C47u /* 'CRLG' */
#define CRASH_HDR_VER 0x0002u

/* ---------- Internal helpers ---------- */

/**
 * @brief Compute CRC32 checksum using standard polynomial 0xEDB88320.
 *
 * Calculates a 32-bit CRC checksum over the provided data buffer using the
 * standard CRC-32 polynomial (Ethernet/ZIP). The implementation uses a
 * bit-by-bit algorithm suitable for embedded systems without lookup tables.
 *
 * @param p Pointer to data buffer to checksum
 * @param len Length of data buffer in bytes
 *
 * @return 32-bit CRC checksum value
 *
 * @note This is a private helper function used for crash log integrity validation.
 */
static uint32_t crash_crc32(const void *p, size_t len) {
    const uint8_t *b = (const uint8_t *)p;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= b[i];
        for (int k = 0; k < 8; k++) {
            uint32_t mask = -(int)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

/**
 * @brief Validate retained crash log header and CRC32 checksum.
 *
 * Checks the crash log structure header for correct magic value, version,
 * and size fields. If basic header validation passes, computes and verifies
 * the CRC32 checksum of the entire structure.
 *
 * @return true if header and CRC are valid, false if initialization needed
 *
 * @note CRC validation temporarily zeros the hdr_crc32 field during computation.
 */
static bool crash_validate_header(void) {
    if (s_crash.hdr_magic != CRASH_HDR_MAGIC)
        return false;
    if (s_crash.hdr_version != CRASH_HDR_VER)
        return false;
    if (s_crash.hdr_size != sizeof(crashlog_t))
        return false;

    uint32_t saved_crc = s_crash.hdr_crc32;
    s_crash.hdr_crc32 = 0u;
    uint32_t calc = crash_crc32(&s_crash, sizeof(s_crash));
    s_crash.hdr_crc32 = saved_crc;
    return (saved_crc == calc);
}

/**
 * @brief Recompute and store updated CRC32 for the crash log structure.
 *
 * Recalculates the CRC32 checksum over the entire crash log structure and
 * updates the hdr_crc32 field. Should be called after any modification to
 * the crash log contents.
 *
 * @return None
 */
static void crash_update_crc(void) {
    s_crash.hdr_crc32 = 0u;
    s_crash.hdr_crc32 = crash_crc32(&s_crash, sizeof(s_crash));
}

/**
 * @brief Initialize retained crash log structure to clean state.
 *
 * Zeros the entire crash log structure and initializes the header with current
 * magic value, version, and size. Sets all diagnostic fields to default values
 * and computes initial CRC32 checksum.
 *
 * This function is called on first boot or when header validation fails due to
 * corruption or version mismatch.
 *
 * @return None
 */
static void crash_fresh_init(void) {
    for (size_t i = 0; i < sizeof(s_crash); i++) {
        ((uint8_t *)&s_crash)[i] = 0u;
    }
    s_crash.hdr_magic = CRASH_HDR_MAGIC;
    s_crash.hdr_version = CRASH_HDR_VER;
    s_crash.hdr_size = (uint16_t)sizeof(crashlog_t);

    s_crash.hf_valid = 0u;
    s_crash.reset_raw_bits = 0u;
    s_crash.reset_reason = CR_RESET_UNKNOWN;
    s_crash.boots_counter = 0u;

    s_crash.wdt_last_ts_ms = 0u;
    s_crash.wdt_feed_wr = 0u;
    s_crash.wdt_feed_count = 0u;
    s_crash.sw_reboot_tag[0] = '\0';

    crash_update_crc();
}

/**
 * @brief Clamp watchdog feed ring buffer indices to safe bounds.
 *
 * Ensures that ring buffer indices and count remain within valid ranges after
 * potential corruption or overflow conditions. Prevents array access violations.
 *
 * @return None
 */
static void crash_clamp_ring(void) {
    if (s_crash.wdt_feed_count > CRASH_FEED_RING)
        s_crash.wdt_feed_count = CRASH_FEED_RING;
    s_crash.wdt_feed_wr = (uint8_t)(s_crash.wdt_feed_wr % CRASH_FEED_RING);
}

/* ---------- Public API ---------- */

void CrashLog_OnHardFault(uint32_t *sp) {
    /* Ensure crash log is initialized before capturing fault context */
    if (!crash_validate_header()) {
        crash_fresh_init();
    }

    /* Mark HardFault as valid and capture stacked register context */
    s_crash.hf_valid = 1u;
    s_crash.r0 = sp[0];
    s_crash.r1 = sp[1];
    s_crash.r2 = sp[2];
    s_crash.r3 = sp[3];
    s_crash.r12 = sp[4];
    s_crash.lr = sp[5];
    s_crash.pc = sp[6];
    s_crash.xpsr = sp[7];

    /* Update CRC to persist fault context across reboot */
    crash_update_crc();

#if defined(NDEBUG)
    /* Release build: log and reboot immediately */
    CRASH_LOG("[CRASHLOG] HardFault captured, rebooting\r\n");
    Health_RebootNow("HardFault crash");
    for (;;) {
        __asm volatile("nop");
    }
#else
    /* Debug build: trigger breakpoint before reboot */
    __asm volatile("bkpt #0");
    Health_RebootNow("HardFault crash");
    for (;;) {
        __asm volatile("nop");
    }
#endif
}

void CrashLog_CaptureResetReasonEarly(void) {
    /* Validate or initialize crash log structure */
    if (!crash_validate_header()) {
        crash_fresh_init();
    }

    /* Increment boot counter for correlation across resets */
    s_crash.boots_counter += 1u;

    /* Capture platform-specific reset cause */
    s_crash.reset_raw_bits = CrashLog_Platform_ReadResetBits();
    s_crash.reset_reason = CrashLog_Platform_DecodeReset(s_crash.reset_raw_bits);

    /* Ensure ring buffer indices remain valid */
    crash_clamp_ring();

    /* Persist updated boot information */
    crash_update_crc();
}

void CrashLog_RecordWdtFeed(uint32_t now_ms) {
    /* Ensure crash log is initialized */
    if (!crash_validate_header()) {
        crash_fresh_init();
    }

    /* Calculate delta from last feed, handle first feed case */
    uint32_t delta = 0u;
    if (s_crash.wdt_last_ts_ms != 0u) {
        delta = (now_ms >= s_crash.wdt_last_ts_ms) ? (now_ms - s_crash.wdt_last_ts_ms) : 0u;
    }
    s_crash.wdt_last_ts_ms = now_ms;

    /* Insert delta into ring buffer at current write position */
    uint8_t wr = (uint8_t)(s_crash.wdt_feed_wr % CRASH_FEED_RING);
    s_crash.wdt_feed_delta_ms[wr] = delta;

    /* Advance write pointer with wraparound */
    s_crash.wdt_feed_wr = (uint8_t)((wr + 1u) % CRASH_FEED_RING);

    /* Increment count until ring is full */
    if (s_crash.wdt_feed_count < CRASH_FEED_RING)
        s_crash.wdt_feed_count++;

    /* Persist telemetry data */
    crash_update_crc();
}

void CrashLog_RecordSoftwareRebootTag(const char *tag) {
    /* Ensure crash log is initialized */
    if (!crash_validate_header()) {
        crash_fresh_init();
    }

    /* Handle NULL tag gracefully */
    if (!tag) {
        return;
    }

    /* Copy tag string with bounds checking */
    size_t i = 0;
    while (tag[i] && i < (sizeof(s_crash.sw_reboot_tag) - 1u)) {
        s_crash.sw_reboot_tag[i] = tag[i];
        i++;
    }

    /* Ensure null termination */
    s_crash.sw_reboot_tag[i] = '\0';

    /* Persist reboot tag */
    crash_update_crc();
}

void CrashLog_PrintAndClearOnBoot(void) {
    /* Wait for logger subsystem to become ready before outputting diagnostics */
    {
        extern bool Logger_IsReady(void);
        const uint32_t timeout_ms = 300u;
        const uint32_t step_ms = 10u;
        uint32_t waited_ms = 0u;

        while (!Logger_IsReady() && waited_ms < timeout_ms) {
#if defined(pdMS_TO_TICKS)
            vTaskDelay(pdMS_TO_TICKS(step_ms));
#else
            /* Fallback delay for non-RTOS environments */
            for (volatile uint32_t i = 0; i < 3000u; ++i) {
                __asm volatile("nop");
            }
#endif
            waited_ms += step_ms;
        }
    }

    /* Validate crash log integrity */
    bool valid = crash_validate_header();

    CRASH_LOG("\r\n[CRASH] Boot diagnostics\r\n");
    if (!valid) {
        CRASH_LOG("  retained=INVALID (reinitialized)\r\n");
        crash_fresh_init();
        valid = true;
    }

    /* Print basic boot information */
    CRASH_LOG("  boots=%lu reason=%u raw=0x%08lx tag=\"%s\"\r\n",
              (unsigned long)s_crash.boots_counter, (unsigned)s_crash.reset_reason,
              (unsigned long)s_crash.reset_raw_bits,
              s_crash.sw_reboot_tag[0] ? s_crash.sw_reboot_tag : "");

    /* Print watchdog feed telemetry if available */
    if (s_crash.wdt_feed_count > 0u) {
        uint8_t count = s_crash.wdt_feed_count;
        uint8_t start =
            (uint8_t)((s_crash.wdt_feed_wr + CRASH_FEED_RING - count) % CRASH_FEED_RING);

        CRASH_LOG("  last_wdt_feed_abs=%lums\r\n", (unsigned long)s_crash.wdt_last_ts_ms);
        CRASH_LOG("  wdt_deltas[%u] (ms):", (unsigned)count);

        /* Output feed deltas in chronological order */
        for (uint8_t i = 0; i < count; i++) {
            uint8_t idx = (uint8_t)((start + i) % CRASH_FEED_RING);
            CRASH_LOG(" %lu", (unsigned long)s_crash.wdt_feed_delta_ms[idx]);
        }
        log_printf("\r\n");
    } else {
        CRASH_LOG("  wdt_deltas[0]\r\n");
    }

    /* Print HardFault context if captured */
    if (s_crash.hf_valid) {
        CRASH_LOG("[CRASH] HardFault captured\r\n");
        CRASH_LOG("  PC=0x%08lx LR=0x%08lx xPSR=0x%08lx\r\n", (unsigned long)s_crash.pc,
                  (unsigned long)s_crash.lr, (unsigned long)s_crash.xpsr);
        CRASH_LOG("  r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r12=0x%08lx\r\n",
                  (unsigned long)s_crash.r0, (unsigned long)s_crash.r1, (unsigned long)s_crash.r2,
                  (unsigned long)s_crash.r3, (unsigned long)s_crash.r12);

        /* Clear HardFault flag after printing */
        s_crash.hf_valid = 0u;
        crash_update_crc();
    }
}

/* ---------- Platform hooks (weak defaults) ---------- */

__attribute__((weak)) uint32_t CrashLog_Platform_ReadResetBits(void) {
#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(PICO_SDK_VERSION_MAJOR)
    uint32_t bits = 0u;

#if defined(PICO_RP2040)
    if (watchdog_caused_reboot()) {
        /* Check scratch registers for intentional reboot signature */
        uint32_t magic = watchdog_hw->scratch[0];
        uint32_t cause = watchdog_hw->scratch[1];

        if (magic == 0x484C5448u && cause == 2u) {
            /* Health module intentional reboot marker detected */
            bits |= 0x2u;
        } else {
            /* Watchdog timeout occurred (fault condition) */
            bits |= 0x1u;
        }
    }
#else
    /* RP2350 or other platforms: simple watchdog detection */
    if (watchdog_caused_reboot()) {
        bits |= 0x1u;
    }
#endif

    /* Check for software reboot tag presence */
    if (s_crash.sw_reboot_tag[0] != '\0') {
        bits |= 0x2u;
    }
    return bits;
#else
    /* Non-RP platforms return no reset information */
    return 0u;
#endif
}

__attribute__((weak)) crash_reset_reason_t CrashLog_Platform_DecodeReset(uint32_t raw) {
#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(PICO_SDK_VERSION_MAJOR)
    /* Decode RP2040/RP2350 reset bits with priority order */
    if (raw & 0x1u)
        return CR_RESET_WATCHDOG; /* Watchdog fault */
    if (raw & 0x2u)
        return CR_RESET_SOFTWARE; /* Intentional software reboot */

    /* No specific reset flags set: assume power-on */
    return CR_RESET_POWERON;
#else
    /* Non-RP platforms cannot determine reset reason */
    (void)raw;
    return CR_RESET_UNKNOWN;
#endif
}
