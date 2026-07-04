/**
 * @file src/tasks/storage_submodule/auth_store.c
 * @brief EEPROM read/write for authentication configuration.
 *
 * Layout: enabled(1) + password(32) + CRC(1) = 34 bytes at 0x2200.
 * Follows the same CRC-8 pattern as user_prefs.c.
 */

#include "../../CONFIG.h"

#define ST_AUTH_TAG "[ST-AUTH]"
#define AUTH_PW_SIZE 32

/** Default auth config: enabled with password "admin". */
static const auth_config_t DEFAULT_AUTH_CONFIG = {
    .enabled = 1,
    .password = "admin",
};

int EEPROM_WriteAuthConfigWithChecksum(const auth_config_t *cfg) {
    if (!cfg) {
        return -1;
    }

    uint8_t buffer[34];
    buffer[0] = cfg->enabled;
    memcpy(&buffer[1], cfg->password, AUTH_PW_SIZE);
    buffer[33] = calculate_crc8(buffer, 33);

    return CAT24C256_WriteBuffer(EEPROM_AUTH_START, buffer, sizeof(buffer));
}

int EEPROM_ReadAuthConfigWithChecksum(auth_config_t *cfg) {
    if (!cfg) {
        return -1;
    }

    uint8_t buffer[34];
    CAT24C256_ReadBuffer(EEPROM_AUTH_START, buffer, sizeof(buffer));

    /* Detect uninitialized EEPROM (all 0xFF) */
    bool all_ff = true;
    for (size_t i = 0; i < sizeof(buffer); i++) {
        if (buffer[i] != 0xFF) {
            all_ff = false;
            break;
        }
    }
    if (all_ff) {
        return -1;
    }

    /* Validate CRC */
    uint8_t crc = calculate_crc8(buffer, 33);
    if (crc != buffer[33]) {
#if ERRORLOGGER
        uint16_t err_code = ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_WARNING, ERR_FID_ST_AUTH, 0x0);
        WARNING_PRINT_CODE(err_code, "%s CRC mismatch\r\n", ST_AUTH_TAG);
        Storage_EnqueueWarningCode(err_code);
#endif
        return -1;
    }

    cfg->enabled = buffer[0];
    memcpy(cfg->password, &buffer[1], AUTH_PW_SIZE);
    cfg->password[AUTH_PW_SIZE - 1] = '\0';

    return 0;
}

auth_config_t LoadAuthConfig(void) {
    auth_config_t cfg;

    if (EEPROM_ReadAuthConfigWithChecksum(&cfg) == 0) {
        INFO_PRINT("%s Loaded auth config from EEPROM\r\n", ST_AUTH_TAG);
        return cfg;
    }

    /* Region uninitialised or corrupt - seed defaults into EEPROM so they
     * persist across reboots without requiring a full factory-defaults cycle. */
    WARNING_PRINT("%s Auth region empty/corrupt, writing defaults\r\n", ST_AUTH_TAG);
    EEPROM_WriteDefaultAuthConfig();
    return DEFAULT_AUTH_CONFIG;
}

int EEPROM_WriteDefaultAuthConfig(void) {
    return EEPROM_WriteAuthConfigWithChecksum(&DEFAULT_AUTH_CONFIG);
}
