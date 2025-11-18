/**
 * @file src/misc/crashlog.c
 * @author DvidMakesThings - David Sipos
 * 
 * @version 1.0.0
 * @date 2025-11-06
 * 
 * @details Minimal HardFault + reset and watchdog feed crash log kept in 
 * retained RAM with integrity. 
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
 * @brief Compute CRC32 (poly 0xEDB88320) over a byte buffer.
 * @param p Pointer to data
 * @param len Length in bytes
 * @return CRC32 value
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
 * @brief Validate retained header and CRC.
 * @return true if valid, false if needs initialization
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
 * @brief Recompute and store header CRC.
 */
static void crash_update_crc(void) {
    s_crash.hdr_crc32 = 0u;
    s_crash.hdr_crc32 = crash_crc32(&s_crash, sizeof(s_crash));
}

/**
 * @brief Initialize retained structure to a known state and stamp header.
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
 * @brief Clamp ring indexes to safe bounds.
 */
static void crash_clamp_ring(void) {
    if (s_crash.wdt_feed_count > CRASH_FEED_RING)
        s_crash.wdt_feed_count = CRASH_FEED_RING;
    s_crash.wdt_feed_wr = (uint8_t)(s_crash.wdt_feed_wr % CRASH_FEED_RING);
}

/* ---------- Public API ---------- */

void CrashLog_OnHardFault(uint32_t *sp) {
    if (!crash_validate_header()) {
        crash_fresh_init();
    }

    s_crash.hf_valid = 1u;
    s_crash.r0 = sp[0];
    s_crash.r1 = sp[1];
    s_crash.r2 = sp[2];
    s_crash.r3 = sp[3];
    s_crash.r12 = sp[4];
    s_crash.lr = sp[5];
    s_crash.pc = sp[6];
    s_crash.xpsr = sp[7];

    crash_update_crc();

#if defined(NDEBUG)
    CRASH_LOG("[CRASHLOG] HardFault captured, rebooting\r\n");
    Health_RebootNow("HardFault crash");
    for (;;) {
        __asm volatile("nop");
    }
#else
    __asm volatile("bkpt #0");
    Health_RebootNow("HardFault crash");
    for (;;) {
        __asm volatile("nop");
    }
#endif
}

void CrashLog_CaptureResetReasonEarly(void) {
    if (!crash_validate_header()) {
        crash_fresh_init();
    }

    s_crash.boots_counter += 1u;

    s_crash.reset_raw_bits = CrashLog_Platform_ReadResetBits();
    s_crash.reset_reason = CrashLog_Platform_DecodeReset(s_crash.reset_raw_bits);

    crash_clamp_ring();
    crash_update_crc();
}

void CrashLog_RecordWdtFeed(uint32_t now_ms) {
    if (!crash_validate_header()) {
        crash_fresh_init();
    }

    uint32_t delta = 0u;
    if (s_crash.wdt_last_ts_ms != 0u) {
        delta = (now_ms >= s_crash.wdt_last_ts_ms) ? (now_ms - s_crash.wdt_last_ts_ms) : 0u;
    }
    s_crash.wdt_last_ts_ms = now_ms;

    uint8_t wr = (uint8_t)(s_crash.wdt_feed_wr % CRASH_FEED_RING);
    s_crash.wdt_feed_delta_ms[wr] = delta;
    s_crash.wdt_feed_wr = (uint8_t)((wr + 1u) % CRASH_FEED_RING);
    if (s_crash.wdt_feed_count < CRASH_FEED_RING)
        s_crash.wdt_feed_count++;

    crash_update_crc();
}

void CrashLog_RecordSoftwareRebootTag(const char *tag) {
    if (!crash_validate_header()) {
        crash_fresh_init();
    }

    if (!tag) {
        return;
    }
    size_t i = 0;
    while (tag[i] && i < (sizeof(s_crash.sw_reboot_tag) - 1u)) {
        s_crash.sw_reboot_tag[i] = tag[i];
        i++;
    }
    s_crash.sw_reboot_tag[i] = '\0';

    crash_update_crc();
}

void CrashLog_PrintAndClearOnBoot(void) {
    {
        extern bool Logger_IsReady(void);
        const uint32_t timeout_ms = 300u;
        const uint32_t step_ms = 10u;
        uint32_t waited_ms = 0u;
        while (!Logger_IsReady() && waited_ms < timeout_ms) {
#if defined(pdMS_TO_TICKS)
            vTaskDelay(pdMS_TO_TICKS(step_ms));
#else
            for (volatile uint32_t i = 0; i < 3000u; ++i) {
                __asm volatile("nop");
            }
#endif
            waited_ms += step_ms;
        }
    }

    bool valid = crash_validate_header();

    CRASH_LOG("\r\n[CRASH] Boot diagnostics\r\n");
    if (!valid) {
        CRASH_LOG("  retained=INVALID (reinitialized)\r\n");
        crash_fresh_init();
        valid = true;
    }

    CRASH_LOG("  boots=%lu reason=%u raw=0x%08lx tag=\"%s\"\r\n",
              (unsigned long)s_crash.boots_counter, (unsigned)s_crash.reset_reason,
              (unsigned long)s_crash.reset_raw_bits,
              s_crash.sw_reboot_tag[0] ? s_crash.sw_reboot_tag : "");

    if (s_crash.wdt_feed_count > 0u) {
        uint8_t count = s_crash.wdt_feed_count;
        uint8_t start =
            (uint8_t)((s_crash.wdt_feed_wr + CRASH_FEED_RING - count) % CRASH_FEED_RING);

        CRASH_LOG("  last_wdt_feed_abs=%lums\r\n", (unsigned long)s_crash.wdt_last_ts_ms);
        CRASH_LOG("  wdt_deltas[%u] (ms):", (unsigned)count);
        for (uint8_t i = 0; i < count; i++) {
            uint8_t idx = (uint8_t)((start + i) % CRASH_FEED_RING);
            CRASH_LOG(" %lu", (unsigned long)s_crash.wdt_feed_delta_ms[idx]);
        }
        log_printf("\r\n");
    } else {
        CRASH_LOG("  wdt_deltas[0]\r\n");
    }

    if (s_crash.hf_valid) {
        CRASH_LOG("[CRASH] HardFault captured\r\n");
        CRASH_LOG("  PC=0x%08lx LR=0x%08lx xPSR=0x%08lx\r\n", (unsigned long)s_crash.pc,
                  (unsigned long)s_crash.lr, (unsigned long)s_crash.xpsr);
        CRASH_LOG("  r0=0x%08lx r1=0x%08lx r2=0x%08lx r3=0x%08lx r12=0x%08lx\r\n",
                  (unsigned long)s_crash.r0, (unsigned long)s_crash.r1, (unsigned long)s_crash.r2,
                  (unsigned long)s_crash.r3, (unsigned long)s_crash.r12);
        s_crash.hf_valid = 0u;
        crash_update_crc();
    }
}

/* ---------- Platform hooks (weak defaults) ---------- */

__attribute__((weak)) uint32_t CrashLog_Platform_ReadResetBits(void) {
#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(PICO_SDK_VERSION_MAJOR)
    uint32_t bits = 0u;
    if (watchdog_caused_reboot()) {
        bits |= 0x1u;
    }
    if (s_crash.sw_reboot_tag[0] != '\0') {
        bits |= 0x2u;
    }
    return bits;
#else
    return 0u;
#endif
}

__attribute__((weak)) crash_reset_reason_t CrashLog_Platform_DecodeReset(uint32_t raw) {
#if defined(PICO_RP2040) || defined(PICO_RP2350) || defined(PICO_SDK_VERSION_MAJOR)
    if (raw & 0x1u)
        return CR_RESET_WATCHDOG;
    if (raw & 0x2u)
        return CR_RESET_SOFTWARE;
    return CR_RESET_POWERON;
#else
    (void)raw;
    return CR_RESET_UNKNOWN;
#endif
}
