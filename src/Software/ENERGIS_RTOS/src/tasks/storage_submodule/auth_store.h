/**
 * @file src/tasks/storage_submodule/auth_store.h
 * @brief EEPROM storage for authentication configuration.
 *
 * Stores auth enabled flag and password in EEPROM at 0x2200 with CRC-8.
 * Layout: enabled(1) + password(32) + CRC(1) = 34 bytes.
 */

#ifndef AUTH_STORE_H
#define AUTH_STORE_H

#include <stdint.h>

/* auth_config_t is defined in EEPROM_MemoryMap.h (included via CONFIG.h) */

/** EEPROM address for auth config block. */
#define EEPROM_AUTH_START 0x2200
/** Size allocated for auth config block. */
#define EEPROM_AUTH_SIZE 0x0040 /* 64 bytes allocated, 34 used */

/**
 * @brief Write auth config to EEPROM with CRC-8.
 * CRITICAL: Must be called with eepromMtx held!
 * @param cfg Pointer to auth_config_t.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteAuthConfigWithChecksum(const auth_config_t *cfg);

/**
 * @brief Read auth config from EEPROM with CRC-8 validation.
 * CRITICAL: Must be called with eepromMtx held!
 * @param cfg Pointer to auth_config_t to fill.
 * @return 0 on success, -1 on CRC failure or uninitialized.
 */
int EEPROM_ReadAuthConfigWithChecksum(auth_config_t *cfg);

/**
 * @brief Load auth config from EEPROM or return defaults.
 * @return Valid auth_config_t (from EEPROM or defaults).
 */
auth_config_t LoadAuthConfig(void);

/**
 * @brief Write default auth config to EEPROM.
 * @return 0 on success, -1 on error.
 */
int EEPROM_WriteDefaultAuthConfig(void);

#endif /* AUTH_STORE_H */
