/**
 * @file src/tasks/storage_submodule/channel_labels.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup storage09 9. Channel Labels
 * @ingroup storage
 * @brief User-defined labels for output channels
 * @{
 * 
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Manages user-defined text labels for all 8 output channels. Each channel
 * has a fixed-size slot in EEPROM for storing null-terminated label strings.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef CHANNEL_LABELS_H
#define CHANNEL_LABELS_H

#include "../../CONFIG.h"

/**
 * @brief Write null-terminated label string for a channel (stored in fixed slot).
 *
 * Each channel has EEPROM_CH_LABEL_SLOT bytes allocated. Label is copied
 * and padded with zeros to fill the slot completely.
 *
 * @param channel_index Channel index [0..ENERGIS_NUM_CHANNELS-1]
 * @param label Null-terminated label string
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
 * @param out Destination buffer for label
 * @param out_len Size of destination buffer in bytes
 * @warning Must be called with eepromMtx held by the caller.
 * @return 0 on success, -1 on invalid channel or null/zero-sized buffer
 */
int EEPROM_ReadChannelLabel(uint8_t channel_index, char *out, size_t out_len);

/**
 * @brief Clear one channel label slot (fills with zeros).
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

#endif /* CHANNEL_LABELS_H */

/** @} */