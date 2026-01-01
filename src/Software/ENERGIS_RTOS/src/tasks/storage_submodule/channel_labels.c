/**
 * @file src/tasks/storage_submodule/channel_labels.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 2.0.0
 * @date 2025-01-01
 *
 * @details Implementation of channel label management. Each of 8 channels has a fixed
 * EEPROM slot (64 bytes) for storing user-defined text labels (null-terminated strings,
 * max 25 characters). Labels are stored at EEPROM address 0x1A00 with 512 bytes total.
 *
 * Labels are cached in RAM for fast access. EEPROM is only read on startup and
 * written when labels change.
 *
 * Thread Safety:
 * - Low-level EEPROM functions require eepromMtx to be held
 * - High-level storage_* functions are fully thread-safe (use storage queue)
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

#define ST_CH_LABEL_TAG "[ST-CHLAB]"

/** @brief Maximum label length (excluding null terminator). */
#define CHANNEL_LABEL_MAX_LEN 25

/** @brief RAM cache for channel labels (owned by StorageTask). */
static char s_label_cache[ENERGIS_NUM_CHANNELS][CHANNEL_LABEL_MAX_LEN + 1];

/** @brief Flag indicating labels have been loaded from EEPROM. */
static bool s_labels_loaded = false;

/**
 * @brief Calculate EEPROM address for a channel's label slot.
 *
 * Each channel has EEPROM_CH_LABEL_SLOT bytes allocated sequentially.
 *
 * @param channel_index Channel index [0..ENERGIS_NUM_CHANNELS-1]
 * @return EEPROM address for channel's label slot
 */
static inline uint16_t _LabelSlotAddr(uint8_t channel_index) {
    return (uint16_t)(EEPROM_CH_LABEL_START + (uint32_t)channel_index * EEPROM_CH_LABEL_SLOT);
}

/* ##################################################################### */
/*                    LOW-LEVEL EEPROM FUNCTIONS                         */
/*           (require eepromMtx to be held by caller)                    */
/* ##################################################################### */

/**
 * @brief Write channel label to EEPROM.
 *
 * Copies label string to buffer, null-terminates, and pads remaining bytes
 * with zeros to fill the entire slot. Label is truncated to CHANNEL_LABEL_MAX_LEN.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param channel_index Channel index [0..7]
 * @param label Null-terminated label string
 * @return 0 on success, -1 on invalid channel or null label
 */
int EEPROM_WriteChannelLabel(uint8_t channel_index, const char *label) {
    /* Validate inputs */
    if (channel_index >= ENERGIS_NUM_CHANNELS || label == NULL) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CHANNEL_LBL, 0x0);
        ERROR_PRINT("%s EEPROM_WriteChannelLabel invalid input: ch=%u, label=%p\r\n",
                    ST_CH_LABEL_TAG, channel_index, (void *)label);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    uint8_t buf[EEPROM_CH_LABEL_SLOT];

    /* Limit to max label length (25 chars + null) */
    size_t maxcpy = CHANNEL_LABEL_MAX_LEN;

    /* Copy label string (up to maxcpy characters) */
    size_t n = 0u;
    for (; n < maxcpy && label[n] != '\0'; ++n)
        buf[n] = (uint8_t)label[n];

    /* Add null terminator */
    buf[n++] = 0x00;

    /* Pad remaining slot with zeros */
    for (; n < EEPROM_CH_LABEL_SLOT; ++n)
        buf[n] = 0x00;

    return CAT24C256_WriteBuffer(_LabelSlotAddr(channel_index), buf, EEPROM_CH_LABEL_SLOT);
}

/**
 * @brief Read channel label from EEPROM.
 *
 * Reads from channel's slot and copies to output buffer, ensuring null termination.
 * Stops at first null byte or end of slot.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param channel_index Channel index [0..7]
 * @param out Destination buffer
 * @param out_len Size of destination buffer
 * @return 0 on success, -1 on invalid channel or null/empty buffer
 */
int EEPROM_ReadChannelLabel(uint8_t channel_index, char *out, size_t out_len) {
    /* Validate inputs */
    if (channel_index >= ENERGIS_NUM_CHANNELS || out == NULL || out_len == 0u) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CHANNEL_LBL, 0x1);
        ERROR_PRINT("%s EEPROM_ReadChannelLabel invalid input: ch=%u, out=%p, out_len=%u\r\n",
                    ST_CH_LABEL_TAG, channel_index, (void *)out, (unsigned)out_len);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    uint8_t buf[EEPROM_CH_LABEL_SLOT];
    CAT24C256_ReadBuffer(_LabelSlotAddr(channel_index), buf, EEPROM_CH_LABEL_SLOT);

    /* Check for uninitialized EEPROM (all 0xFF) */
    if (buf[0] == 0xFF) {
        out[0] = '\0';
        return 0;
    }

    /* Copy to output buffer until null byte, buffer full, or slot end */
    size_t max_copy = (out_len < CHANNEL_LABEL_MAX_LEN + 1) ? out_len - 1 : CHANNEL_LABEL_MAX_LEN;
    size_t i = 0u;
    while (i < max_copy && i < EEPROM_CH_LABEL_SLOT && buf[i] != 0x00) {
        out[i] = (char)buf[i];
        ++i;
    }

    /* Ensure null termination */
    out[i] = '\0';

    return 0;
}

/**
 * @brief Clear one channel label slot by filling with zeros.
 *
 * Writes zeros in 32-byte chunks to erase the entire label slot.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param channel_index Channel index [0..7]
 * @return 0 on success, -1 on invalid channel or I2C error
 */
int EEPROM_ClearChannelLabel(uint8_t channel_index) {
    /* Validate channel index */
    if (channel_index >= ENERGIS_NUM_CHANNELS) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CHANNEL_LBL, 0x2);
        ERROR_PRINT("%s EEPROM_ClearChannelLabel invalid channel: ch=%u\r\n", ST_CH_LABEL_TAG,
                    channel_index);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    uint8_t zero[32];
    memset(zero, 0x00, sizeof(zero));

    /* Clear slot in 32-byte chunks */
    for (uint16_t addr = 0; addr < EEPROM_CH_LABEL_SLOT; addr += sizeof(zero)) {
        if (CAT24C256_WriteBuffer(_LabelSlotAddr(channel_index) + addr, zero, sizeof(zero)) != 0)
            return -1;
    }

    return 0;
}

/**
 * @brief Clear all channel label slots.
 *
 * Iterates through all channels and clears their label slots.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @return 0 on success, -1 if any channel clear fails
 */
int EEPROM_ClearAllChannelLabels(void) {
    for (uint8_t ch = 0; ch < ENERGIS_NUM_CHANNELS; ++ch) {
        if (EEPROM_ClearChannelLabel(ch) != 0) {
#if ERRORLOGGER
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_CHANNEL_LBL, 0x3);
            ERROR_PRINT("%s EEPROM_ClearAllChannelLabels failed on channel: ch=%u\r\n",
                        ST_CH_LABEL_TAG, ch);
            Storage_EnqueueErrorCode(err_code);
#endif
            return -1;
        }
    }
    return 0;
}

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
 * CRITICAL: Must be called with eepromMtx held!
 */
void ChannelLabels_LoadFromEEPROM(void) {
    for (uint8_t ch = 0; ch < ENERGIS_NUM_CHANNELS; ++ch) {
        EEPROM_ReadChannelLabel(ch, s_label_cache[ch], sizeof(s_label_cache[ch]));
    }
    s_labels_loaded = true;
    INFO_PRINT("%s Labels loaded into cache\r\n", ST_CH_LABEL_TAG);
}

/**
 * @brief Get label from RAM cache (non-blocking).
 *
 * Returns cached label without EEPROM access.
 *
 * @param channel Channel index [0..7]
 * @param out Destination buffer
 * @param out_len Size of destination buffer
 * @return 0 on success, -1 on invalid parameters
 */
int ChannelLabels_GetCached(uint8_t channel, char *out, size_t out_len) {
    if (channel >= ENERGIS_NUM_CHANNELS || out == NULL || out_len == 0) {
        return -1;
    }

    if (!s_labels_loaded) {
        out[0] = '\0';
        return -1;
    }

    /* Copy from cache */
    size_t copy_len = strlen(s_label_cache[channel]);
    if (copy_len >= out_len) {
        copy_len = out_len - 1;
    }
    memcpy(out, s_label_cache[channel], copy_len);
    out[copy_len] = '\0';

    return 0;
}

/**
 * @brief Update label in cache and write to EEPROM.
 *
 * Updates the RAM cache immediately and writes to EEPROM.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param channel Channel index [0..7]
 * @param label New label string
 * @return 0 on success, -1 on error
 */
int ChannelLabels_SetAndWrite(uint8_t channel, const char *label) {
    if (channel >= ENERGIS_NUM_CHANNELS || label == NULL) {
        return -1;
    }

    /* Update cache first */
    size_t i;
    for (i = 0; i < CHANNEL_LABEL_MAX_LEN && label[i] != '\0'; ++i) {
        s_label_cache[channel][i] = label[i];
    }
    s_label_cache[channel][i] = '\0';

    /* Write to EEPROM */
    return EEPROM_WriteChannelLabel(channel, label);
}

/* ##################################################################### */
/*                  HIGH-LEVEL THREAD-SAFE FUNCTIONS                     */
/*            (use storage queue, safe to call from any task)            */
/* ##################################################################### */

/**
 * @brief Read channel label from RAM cache (thread-safe, non-blocking).
 *
 * Returns cached label without storage queue or EEPROM access.
 * Safe to call from any task context.
 *
 * @param channel Channel index [0..7]
 * @param out Destination buffer (minimum 26 bytes recommended)
 * @param out_len Size of destination buffer
 * @return true on success, false on error
 */
bool storage_get_channel_label(uint8_t channel, char *out, size_t out_len) {
    if (!out || out_len == 0 || channel >= ENERGIS_NUM_CHANNELS) {
        return false;
    }

    if (!Storage_Config_IsReady()) {
        out[0] = '\0';
        return false;
    }

    return (ChannelLabels_GetCached(channel, out, out_len) == 0);
}

/**
 * @brief Write channel label via storage queue (thread-safe).
 *
 * Enqueues a write request to StorageTask. The write updates the RAM cache
 * immediately and writes to EEPROM asynchronously.
 *
 * @param channel Channel index [0..7]
 * @param label Label string (max 25 characters, truncated if longer)
 * @return true if request was queued, false on error
 */
bool storage_set_channel_label(uint8_t channel, const char *label) {
    extern QueueHandle_t q_cfg;

    if (!label || channel >= ENERGIS_NUM_CHANNELS || !Storage_Config_IsReady()) {
        return false;
    }

    storage_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = STORAGE_CMD_WRITE_CHANNEL_LABEL;
    msg.data.ch_label.channel = channel;
    msg.done_sem = NULL; /* Async write */

    /* Copy label with truncation */
    size_t i;
    for (i = 0; i < CHANNEL_LABEL_MAX_LEN && label[i] != '\0'; ++i) {
        msg.data.ch_label.label[i] = label[i];
    }
    msg.data.ch_label.label[i] = '\0';

    return (xQueueSend(q_cfg, &msg, pdMS_TO_TICKS(1000)) == pdPASS);
}

/**
 * @brief Read all channel labels from RAM cache (thread-safe, non-blocking).
 *
 * Convenience function to read all 8 channel labels at once from cache.
 *
 * @param labels Array of 8 label buffers, each at least 26 bytes
 * @param label_buf_size Size of each label buffer
 * @return true if all labels read successfully, false on any error
 */
bool storage_get_all_channel_labels(char labels[ENERGIS_NUM_CHANNELS][26], size_t label_buf_size) {
    if (!labels || label_buf_size < 2) {
        return false;
    }

    bool all_ok = true;
    for (uint8_t ch = 0; ch < ENERGIS_NUM_CHANNELS; ++ch) {
        if (!storage_get_channel_label(ch, labels[ch], label_buf_size)) {
            labels[ch][0] = '\0';
            all_ok = false;
        }
    }
    return all_ok;
}