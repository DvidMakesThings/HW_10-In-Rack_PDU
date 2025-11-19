/**
 * @file src/tasks/storage_submodule/channel_labels.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of channel label management. Each of 8 channels has a fixed
 * EEPROM slot for storing user-defined text labels (null-terminated strings).
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../../CONFIG.h"

#define ST_CH_LABEL_TAG "[ST-CHLAB]"

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

/**
 * @brief Write channel label to EEPROM.
 *
 * Copies label string to buffer, null-terminates, and pads remaining bytes
 * with zeros to fill the entire slot.
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
    size_t maxcpy = EEPROM_CH_LABEL_SLOT - 1u; /* Reserve 1 byte for null terminator */

    /* Copy label string (up to maxcpy characters) */
    size_t n = 0u;
    for (; n < maxcpy && label[n] != '\0'; ++n)
        buf[n] = (uint8_t)label[n];

    /* Add null terminator */
    buf[n++] = 0x00;

    /* Pad remaining slot with zeros */
    for (; n < EEPROM_CH_LABEL_SLOT; ++n)
        buf[n] = 0x00;

    /* Write entire slot to EEPROM */
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

    /* Copy to output buffer until null byte, buffer full, or slot end */
    size_t i = 0u;
    while (i + 1u < out_len && i < EEPROM_CH_LABEL_SLOT && buf[i] != 0x00) {
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