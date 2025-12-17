/**
 * @file src/tasks/storage_submodule/network.c
 * @author DvidMakesThings - David Sipos
 *
 * @version 1.0
 * @date 2025-11-14
 *
 * @details Implementation of network configuration management with CRC validation.
 * Handles IP, MAC, gateway, DNS settings with automatic MAC repair for corrupted values.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "network.h"

/* External declarations from StorageTask.c */
extern const networkInfo DEFAULT_NETWORK;

#define ST_NETWORK_TAG "[ST-NET]"

/* ==================== System Info Functions ==================== */

/**
 * @brief Write system info block (serial number, software version, etc).
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Source buffer containing system info
 * @param len Number of bytes to write
 * @return 0 on success, -1 on bounds check failure or I2C error
 */
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x1);
        ERROR_PRINT_CODE(err_code, "%s EEPROM_WriteSystemInfo: Write length exceeds size\r\n",
                         ST_NETWORK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    return CAT24C256_WriteBuffer(EEPROM_SYS_INFO_START, data, (uint16_t)len);
}

/**
 * @brief Read system info block.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len Number of bytes to read
 * @return 0 on success, -1 on bounds check failure
 */
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x2);
        ERROR_PRINT_CODE(err_code, "%s EEPROM_ReadSystemInfo: Read length exceeds size\r\n",
                         ST_NETWORK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    CAT24C256_ReadBuffer(EEPROM_SYS_INFO_START, data, (uint32_t)len);
    return 0;
}

/**
 * @brief Write system info with CRC-8 appended for validation.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Source buffer
 * @param len Data length (reserves 1 byte for CRC)
 * @return 0 on success, -1 on error
 */
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x3);
        ERROR_PRINT_CODE(err_code, "%s Write length exceeds size\r\n", ST_NETWORK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    uint8_t buffer[EEPROM_SYS_INFO_SIZE];
    memcpy(buffer, data, len);
    buffer[len] = calculate_crc8(data, len);

    return CAT24C256_WriteBuffer(EEPROM_SYS_INFO_START, buffer, (uint16_t)(len + 1));
}

/**
 * @brief Read and verify system info with CRC-8.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len Data length (CRC byte excluded)
 * @return 0 if CRC OK, -1 on CRC mismatch
 */
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x4);
        ERROR_PRINT_CODE(err_code, "%s Read length exceeds size\r\n", ST_NETWORK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    uint8_t buffer[EEPROM_SYS_INFO_SIZE];

    CAT24C256_ReadBuffer(EEPROM_SYS_INFO_START, buffer, (uint32_t)(len + 1));

    uint8_t crc = calculate_crc8(buffer, len);
    if (crc != buffer[len]) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x5);
        ERROR_PRINT_CODE(err_code, "%s CRC mismatch (calculated 0x%02X, stored 0x%02X)\r\n",
                         ST_NETWORK_TAG, crc, buffer[len]);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    memcpy(data, buffer, len);
    return 0;
}

/* ==================== Network Configuration Functions ==================== */

/**
 * @brief Write raw network configuration (no CRC).
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Source buffer
 * @param len Number of bytes to write
 * @return 0 on success, -1 on error
 */
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x6);
        ERROR_PRINT_CODE(err_code, "%s Write length exceeds size\r\n", ST_NETWORK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    return CAT24C256_WriteBuffer(EEPROM_USER_NETWORK_START, data, (uint16_t)len);
}

/**
 * @brief Read raw network configuration (no CRC).
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param data Destination buffer
 * @param len Number of bytes to read
 * @return 0 on success, -1 on error
 */
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x7);
        ERROR_PRINT_CODE(err_code, "%s Read length exceeds size\r\n", ST_NETWORK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    CAT24C256_ReadBuffer(EEPROM_USER_NETWORK_START, data, (uint32_t)len);
    return 0;
}

/**
 * @brief Write network configuration with CRC-8 validation.
 *
 * Layout: MAC(6) + IP(4) + SN(4) + GW(4) + DNS(4) + DHCP(1) + CRC(1) = 24 bytes
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param net_info Network configuration structure
 * @return 0 on success, -1 on null pointer
 */
int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info) {
    if (!net_info) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x8);
        ERROR_PRINT_CODE(err_code,
                         "%s Null pointer provided to EEPROM_WriteUserNetworkWithChecksum\r\n",
                         ST_NETWORK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    uint8_t buffer[24];

    /* Pack network info into buffer */
    memcpy(&buffer[0], net_info->mac, 6);
    memcpy(&buffer[6], net_info->ip, 4);
    memcpy(&buffer[10], net_info->sn, 4);
    memcpy(&buffer[14], net_info->gw, 4);
    memcpy(&buffer[18], net_info->dns, 4);
    buffer[22] = net_info->dhcp;

    /* Calculate and append CRC */
    buffer[23] = calculate_crc8(buffer, 23);

    return CAT24C256_WriteBuffer(EEPROM_USER_NETWORK_START, buffer, 24);
}

/**
 * @brief Read and verify network configuration with CRC-8.
 *
 * CRITICAL: Must be called with eepromMtx held!
 *
 * @param net_info Destination structure
 * @return 0 if CRC OK, -1 on CRC mismatch or null pointer
 */
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info) {
    if (!net_info) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0x9);
        ERROR_PRINT_CODE(err_code,
                         "%s Null pointer provided to EEPROM_ReadUserNetworkWithChecksum\r\n",
                         ST_NETWORK_TAG);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    uint8_t buffer[24];

    CAT24C256_ReadBuffer(EEPROM_USER_NETWORK_START, buffer, 24);

    /* Verify CRC */
    if (calculate_crc8(buffer, 23) != buffer[23]) {
#if ERRORLOGGER
        uint16_t err_code =
            ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_ERROR, ERR_FID_ST_NETWORK_CFG, 0xA);
        ERROR_PRINT_CODE(err_code, "%s CRC mismatch (calculated 0x%02X, stored 0x%02X)\r\n",
                         ST_NETWORK_TAG, calculate_crc8(buffer, 23), buffer[23]);
        Storage_EnqueueErrorCode(err_code);
#endif
        return -1;
    }

    /* Unpack network info from buffer */
    memcpy(net_info->mac, &buffer[0], 6);
    memcpy(net_info->ip, &buffer[6], 4);
    memcpy(net_info->sn, &buffer[10], 4);
    memcpy(net_info->gw, &buffer[14], 4);
    memcpy(net_info->dns, &buffer[18], 4);
    net_info->dhcp = buffer[22];

    return 0;
}

/**
 * @brief Load network configuration from EEPROM or defaults.
 *
 * Reads network config with CRC validation. If successful, repairs MAC if corrupted.
 * If CRC fails or empty, returns defaults with derived MAC and persists them.
 *
 * @return Valid networkInfo structure (either from EEPROM or defaults)
 */
networkInfo LoadUserNetworkConfig(void) {
    networkInfo net_info;

    /* Attempt to read from EEPROM with CRC validation */
    if (EEPROM_ReadUserNetworkWithChecksum(&net_info) == 0) {
        /* CRC OK - check and repair MAC if needed */
        if (Energis_RepairMac(&net_info)) {
#if ERRORLOGGER
            uint16_t err_code =
                ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_WARNING, ERR_FID_ST_NETWORK_CFG, 0x1);
            WARNING_PRINT_CODE(err_code,
                               "%s Network MAC address was corrupted and has been repaired to "
                               "%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                               ST_NETWORK_TAG, net_info.mac[0], net_info.mac[1], net_info.mac[2],
                               net_info.mac[3], net_info.mac[4], net_info.mac[5]);
            Storage_EnqueueWarningCode(err_code);
#endif
            /* Persist repaired MAC */
            (void)EEPROM_WriteUserNetworkWithChecksum(&net_info);
        }
        return net_info;
    }

/* CRC failed or empty EEPROM - use defaults */
#if ERRORLOGGER
    uint16_t err_code =
        ERR_MAKE_CODE(ERR_MOD_STORAGE, ERR_SEV_WARNING, ERR_FID_ST_NETWORK_CFG, 0x2);
    WARNING_PRINT_CODE(err_code, "%s Failed to read valid network config from EEPROM\r\n",
                       ST_NETWORK_TAG);
    Storage_EnqueueWarningCode(err_code);
#endif
    net_info = DEFAULT_NETWORK;
    Energis_FillMacFromSerial(net_info.mac);

    /* Persist defaults */
    (void)EEPROM_WriteUserNetworkWithChecksum(&net_info);

    return net_info;
}