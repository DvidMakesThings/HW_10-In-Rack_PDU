/**
 * @file src/tasks/storage_submodule/channel_labels.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage09 9. Channel Labels
 * @ingroup storage
 * @brief User-defined labels for output channels
 * @{
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details Manages user-defined text labels for all 8 output channels. Each channel
 * has a fixed-size slot (64 bytes) in EEPROM for storing null-terminated label strings.
 * Maximum label length is 25 characters.
 *
 * Labels are cached in RAM for fast access. EEPROM is only read on startup
 * (via ChannelLabels_LoadFromEEPROM) and written when labels change.
 *
 * EEPROM Layout (0x1A00 - 0x1BFF, 512 bytes total):
 * - Channel 0: 0x1A00 - 0x1A3F (64 bytes)
 * - Channel 1: 0x1A40 - 0x1A7F (64 bytes)
 * - Channel 2: 0x1A80 - 0x1ABF (64 bytes)
 * - Channel 3: 0x1AC0 - 0x1AFF (64 bytes)
 * - Channel 4: 0x1B00 - 0x1B3F (64 bytes)
 * - Channel 5: 0x1B40 - 0x1B7F (64 bytes)
 * - Channel 6: 0x1B80 - 0x1BBF (64 bytes)
 * - Channel 7: 0x1BC0 - 0x1BFF (64 bytes)
 *
 * Thread Safety:
 * - Low-level EEPROM_* functions require eepromMtx to be held by caller
 * - ChannelLabels_* cache functions are called by StorageTask
 * - High-level storage_* functions are fully thread-safe
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CHANNEL_LABELS_H
#define CHANNEL_LABELS_H

#include "../../CONFIG.h"

/* ##################################################################### */
/*                    LOW-LEVEL EEPROM FUNCTIONS                         */
/*           (require eepromMtx to be held by caller)                    */
/* ##################################################################### */

/**
 * @brief Write null-terminated label string for a channel (stored in fixed slot).
 *
 * Each channel has EEPROM_CH_LABEL_SLOT bytes allocated. Label is copied
 * and padded with zeros to fill the slot completely. Max 25 characters.
 *
 * @param channel_index Channel index [0..ENERGIS_NUM_CHANNELS-1]
 * @param label Null-terminated label string (max 25 chars)
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null input
 */
int EEPROM_WriteChannelLabel(uint8_t channel_index, const char *label);

/**
 * @brief Read label string for a channel.
 *
 * Reads from channel's fixed EEPROM slot and ensures output is null-terminated.
 *
 * @param channel_index Channel index [0..ENERGIS_NUM_CHANNELS-1]
 * @param out Destination buffer for label (26 bytes recommended)
 * @param out_len Size of destination buffer in bytes
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null/zero-sized buffer
 */
int EEPROM_ReadChannelLabel(uint8_t channel_index, char *out, size_t out_len);

/**
 * @brief Clear one channel label slot (fills with zeros).
 *
 * @param channel_index Channel index [0..ENERGIS_NUM_CHANNELS-1]
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or write error
 */
int EEPROM_ClearChannelLabel(uint8_t channel_index);

/**
 * @brief Clear all channel label slots.
 *
 * Iterates through all channels and clears their label slots.
 *
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on write error
 */
int EEPROM_ClearAllChannelLabels(void);

/* ##################################################################### */
/*                     RAM CACHE MANAGEMENT                              */
/*           (called by StorageTask during startup/write)                */
/* ##################################################################### */

/**
 * @brief Load all channel labels from EEPROM into RAM cache.
 *
 * Called once during StorageTask initialization. After this, all reads
 * are served from cache.
 *
 * @warning Must be called with eepromMtx held by the caller.
 */
void ChannelLabels_LoadFromEEPROM(void);

/**
 * @brief Get label from RAM cache (non-blocking).
 *
 * Returns cached label without EEPROM access.
 *
 * @param channel Channel index [0..7]
 * @param out Destination buffer
 * @param out_len Size of destination buffer
 * @return 0 on success, -1 on invalid parameters or cache not loaded
 */
int ChannelLabels_GetCached(uint8_t channel, char *out, size_t out_len);

/**
 * @brief Update label in cache and write to EEPROM.
 *
 * Updates the RAM cache immediately and writes to EEPROM.
 *
 * @param channel Channel index [0..7]
 * @param label New label string
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on error
 */
int ChannelLabels_SetAndWrite(uint8_t channel, const char *label);

/* ##################################################################### */
/*                  HIGH-LEVEL THREAD-SAFE FUNCTIONS                     */
/*            (safe to call from any task)                               */
/* ##################################################################### */

/**
 * @brief Read channel label from RAM cache (thread-safe, non-blocking).
 *
 * Returns cached label without storage queue or EEPROM access.
 * Safe to call from any task context including web handlers.
 *
 * @param channel Channel index [0..7]
 * @param out Destination buffer (minimum 26 bytes recommended)
 * @param out_len Size of destination buffer
 * @return true on success, false on error
 */
bool storage_get_channel_label(uint8_t channel, char *out, size_t out_len);

/**
 * @brief Write channel label via storage queue (thread-safe).
 *
 * Enqueues a write request to StorageTask. The write updates the RAM cache
 * and writes to EEPROM.
 *
 * @param channel Channel index [0..7]
 * @param label Label string (max 25 characters, truncated if longer)
 * @return true if request was queued, false on error
 */
bool storage_set_channel_label(uint8_t channel, const char *label);

/**
 * @brief Read all channel labels from RAM cache (thread-safe, non-blocking).
 *
 * Convenience function to read all 8 channel labels at once from cache.
 *
 * @param labels Array of 8 label buffers, each at least 26 bytes
 * @param label_buf_size Size of each label buffer
 * @return true if all labels read successfully, false on any error
 */
bool storage_get_all_channel_labels(char labels[ENERGIS_NUM_CHANNELS][26], size_t label_buf_size);

#endif /* CHANNEL_LABELS_H */

/** @} */