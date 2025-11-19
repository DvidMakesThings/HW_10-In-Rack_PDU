/**
 * @file src/misc/crashlog.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup misc Miscellaneous Utilities
 * @brief Miscellaneous utility functions and modules.
 * @{
 *
 * @defgroup misc1 1. Crash Log Module
 * @ingroup misc
 * @brief Header file for crash log module
 * @{
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

#pragma once

#include "../CONFIG.h"

/** @brief Retained log ring capacity for watchdog feeds. */
#define CRASH_FEED_RING 16u

/**
 * @brief Reset cause enumeration recorded at early boot.
 */
typedef enum {
    CR_RESET_UNKNOWN = 0u,  /**< Unknown or not captured. */
    CR_RESET_POWERON = 1u,  /**< Power-on or external power cycle. */
    CR_RESET_WATCHDOG = 2u, /**< Hardware watchdog caused reboot. */
    CR_RESET_SOFTWARE = 3u, /**< Software-called reboot/reset. */
    CR_RESET_BROWNOUT = 4u, /**< Brown-out or supply dip. */
    CR_RESET_EXTERNAL = 5u, /**< External reset pin toggled. */
    CR_RESET_DEBUG = 6u     /**< Debug probe reset. */
} crash_reset_reason_t;

/**
 * @brief Crash log structure stored in noinit so it survives resets.
 *
 * This structure contains a validated header with versioning and CRC.
 * Only after a successful header validation are the runtime fields considered valid.
 */
typedef struct {
    /* Header with integrity */
    uint32_t hdr_magic;   /**< Magic 'CRLG' to validate retained block. */
    uint16_t hdr_version; /**< Structure version for compatibility. */
    uint16_t hdr_size;    /**< sizeof(crashlog_t) at build time. */
    uint32_t hdr_crc32;   /**< CRC32 of the structure with this field set to 0. */

    /* HardFault capture (valid only if hf_valid is non-zero) */
    uint8_t hf_valid;             /**< Non-zero indicates HardFault registers below are valid. */
    uint8_t rsvd0[3];             /**< Reserved padding. */
    uint32_t pc;                  /**< Program Counter at fault. */
    uint32_t lr;                  /**< Link Register at fault. */
    uint32_t xpsr;                /**< xPSR at fault. */
    uint32_t r0, r1, r2, r3, r12; /**< Stacked registers at fault. */

    /* Reset diagnostics */
    uint32_t reset_raw_bits;           /**< Platform-specific raw reset bits (if captured). */
    crash_reset_reason_t reset_reason; /**< Decoded reset reason. */
    uint32_t boots_counter;            /**< Monotonic boots counter (retained). */

    /* Watchdog feed telemetry as deltas for readability and robustness */
    uint32_t wdt_last_ts_ms;                     /**< Absolute ms of the last recorded feed. */
    uint32_t wdt_feed_delta_ms[CRASH_FEED_RING]; /**< Deltas in ms since previous feed. */
    uint8_t wdt_feed_wr;                         /**< Write index into ring. */
    uint8_t wdt_feed_count; /**< Number of valid entries (<= CRASH_FEED_RING). */
    uint8_t rsvd1[2];       /**< Reserved padding. */

    /* Optional tag for software-initiated reboot */
    char sw_reboot_tag[24]; /**< Short ASCII tag for reason (optional, NUL-terminated). */
} crashlog_t;

/**
 * @brief Print last crash (if any) and reset reason; clears HardFault fields after print.
 *
 * Non-blocking: waits briefly for logger readiness and then dumps best-effort info.
 */
void CrashLog_PrintAndClearOnBoot(void);

/**
 * @brief Record a HW watchdog feed timestamp (call right after feeding the HW WDT).
 * @param now_ms Monotonic milliseconds.
 */
void CrashLog_RecordWdtFeed(uint32_t now_ms);

/**
 * @brief Capture reset cause very early at boot (before RTOS/tasks).
 *        Validates and initializes retained header once per power-on, then increments boots
 * counter.
 */
void CrashLog_CaptureResetReasonEarly(void);

/**
 * @brief Optional: tag a software reboot reason right before calling watchdog_reboot().
 * @param tag Short, null-terminated ASCII (truncated to fit).
 */
void CrashLog_RecordSoftwareRebootTag(const char *tag);

/**
 * @brief Handler entry called from vector HardFault_Handler (C bridge).
 * @param stacked_regs Pointer to stacked registers frame (r0,r1,r2,r3,r12,lr,pc,xpsr).
 */
void CrashLog_OnHardFault(uint32_t *stacked_regs);

/* --- Platform hooks (weak) --- */

/**
 * @brief Platform hook to read raw reset-cause bits (weak; provided for RP2040 by default).
 * @return Raw bits value (platform-specific).
 */
uint32_t CrashLog_Platform_ReadResetBits(void);

/**
 * @brief Platform hook to decode raw bits into @ref crash_reset_reason_t (weak; default heuristic).
 * @param raw Raw bits value from @ref CrashLog_Platform_ReadResetBits.
 * @return Decoded reason.
 */
crash_reset_reason_t CrashLog_Platform_DecodeReset(uint32_t raw);

/** @} */