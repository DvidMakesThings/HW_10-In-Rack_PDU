#include "utils/EEPROM_MemoryMap.h"
#include "drivers/CAT24C512_driver.h"  // Assumes your EEPROM driver functions are available
#include <string.h>
#include <stdio.h>

//----------------------------
// CRC-8 function using polynomial 0x07.
static uint8_t calculate_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

//----------------------------
// Dummy EEPROM content update using CRC for every region.
void write_dummy_eeprom_content(void) {
    // Initialize the EEPROM (if not already done elsewhere)
    CAT24C512_Init();

    // --------- 1. System Info & Calibration ---------
    // We'll store "v2.1.0;SN-369366060325" in 31 bytes; the 32nd byte will be the CRC.
    uint8_t sys_info[31];
    memset(sys_info, 0, sizeof(sys_info));
    const char *info = "v2.1.0;SN-369366060325";
    strncpy((char*)sys_info, info, sizeof(sys_info) - 1);

    // Use the provided functions for system info (which use CRC)
    uint8_t stored_sys_info[31];
    if (EEPROM_ReadSystemInfoWithChecksum(stored_sys_info, sizeof(stored_sys_info)) == 0) {
        if (memcmp(stored_sys_info, sys_info, sizeof(stored_sys_info)) == 0) {
            printf("System info unchanged. Skipping write.\n");
        } else {
            if (EEPROM_WriteSystemInfoWithChecksum(sys_info, sizeof(sys_info)) != 0)
                printf("Failed to update system info.\n");
            else
                printf("System info updated.\n");
        }
    } else {
        if (EEPROM_WriteSystemInfoWithChecksum(sys_info, sizeof(sys_info)) != 0)
            printf("Failed to write system info.\n");
        else
            printf("System info written (CRC error detected, rewriting).\n");
    }
    
    // --------- 2. Factory Default Settings ---------
    // For factory defaults, use the same dummy data as system info.
    {
        const size_t data_len = sizeof(sys_info); // 31 bytes
        uint8_t crc = calculate_crc8(sys_info, data_len);
        uint8_t stored[32];  // 31 data bytes + 1 CRC byte
        CAT24C512_ReadBuffer(EEPROM_FACTORY_DEFAULTS_START, stored, data_len + 1);
        if ((memcmp(stored, sys_info, data_len) == 0) && (stored[data_len] == crc)) {
            printf("Factory defaults unchanged. Skipping write.\n");
        } else {
            uint8_t write_buf[32];
            memcpy(write_buf, sys_info, data_len);
            write_buf[data_len] = crc;
            if (CAT24C512_WriteBuffer(EEPROM_FACTORY_DEFAULTS_START, write_buf, data_len + 1) != 0)
                printf("Failed to write factory defaults.\n");
            else
                printf("Factory defaults written.\n");
        }
    }
    
    // --------- 3. User Output Configuration ---------
    // Default state: all relays off. Use 8 bytes dummy; total stored = 8+1 bytes.
    {
        const size_t data_len = 8;
        uint8_t dummy[data_len];
        memset(dummy, 0, sizeof(dummy));
        uint8_t crc = calculate_crc8(dummy, data_len);
        uint8_t stored[data_len + 1];
        CAT24C512_ReadBuffer(EEPROM_USER_OUTPUT_START, stored, data_len + 1);
        if ((memcmp(stored, dummy, data_len) == 0) && (stored[data_len] == crc)) {
            printf("User output configuration unchanged. Skipping write.\n");
        } else {
            uint8_t write_buf[data_len + 1];
            memcpy(write_buf, dummy, data_len);
            write_buf[data_len] = crc;
            if (CAT24C512_WriteBuffer(EEPROM_USER_OUTPUT_START, write_buf, data_len + 1) != 0)
                printf("Failed to write user output configuration.\n");
            else
                printf("User output configuration written.\n");
        }
    }
    
    // --------- 4. User Network Settings ---------
    // Default IP: 192.168.0.100 and Gateway: 192.168.0.1 (8 bytes total)
    {
        networkInfo default_network = {
            .ip = {192, 168, 0, 11},
            .gw = {192, 168, 0, 1},
            .sn = {255, 255, 255, 0},
            .mac = {0x00, 0x08, 0xDC, 0xBE, 0xEF, 0x91},
            .dns = {8, 8, 8, 8},
            .dhcp = NETINFO_STATIC
        };
    
        networkInfo stored_network;
    
        if (EEPROM_ReadUserNetworkWithChecksum(&stored_network) == 0) {
            if (memcmp(&stored_network, &default_network, sizeof(networkInfo)) != 0) {
                if (EEPROM_WriteUserNetworkWithChecksum(&default_network) != 0)
                    printf("Failed to update user network settings.\n");
                else
                    printf("User network settings updated.\n");
            } else {
                printf("User network settings unchanged. Skipping write.\n");
            }
        } else {
            if (EEPROM_WriteUserNetworkWithChecksum(&default_network) != 0)
                printf("Failed to write user network settings.\n");
            else
                printf("User network settings written.\n");
        }
    }
    
    // --------- 5. Sensor Calibration & Thresholds ---------
    // Dummy data: 64 bytes of zeros; store 64+1 bytes.
    {
        const size_t data_len = 64;
        uint8_t dummy[data_len];
        memset(dummy, 0, sizeof(dummy));
        uint8_t crc = calculate_crc8(dummy, data_len);
        uint8_t stored[data_len + 1];
        CAT24C512_ReadBuffer(EEPROM_SENSOR_CAL_START, stored, data_len + 1);
        if ((memcmp(stored, dummy, data_len) == 0) && (stored[data_len] == crc)) {
            printf("Sensor calibration unchanged. Skipping write.\n");
        } else {
            uint8_t write_buf[data_len + 1];
            memcpy(write_buf, dummy, data_len);
            write_buf[data_len] = crc;
            if (CAT24C512_WriteBuffer(EEPROM_SENSOR_CAL_START, write_buf, data_len + 1) != 0)
                printf("Failed to write sensor calibration.\n");
            else
                printf("Sensor calibration written.\n");
        }
    }
    
    // --------- 6. Energy Monitoring Data ---------
    // Dummy data: 64 bytes of zeros; store 64+1 bytes.
    {
        const size_t data_len = 64;
        uint8_t dummy[data_len];
        memset(dummy, 0, sizeof(dummy));
        uint8_t crc = calculate_crc8(dummy, data_len);
        uint8_t stored[data_len + 1];
        CAT24C512_ReadBuffer(EEPROM_ENERGY_MON_START, stored, data_len + 1);
        if ((memcmp(stored, dummy, data_len) == 0) && (stored[data_len] == crc)) {
            printf("Energy monitoring data unchanged. Skipping write.\n");
        } else {
            uint8_t write_buf[data_len + 1];
            memcpy(write_buf, dummy, data_len);
            write_buf[data_len] = crc;
            if (CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START, write_buf, data_len + 1) != 0)
                printf("Failed to write energy monitoring data.\n");
            else
                printf("Energy monitoring data written.\n");
        }
    }
    
    // --------- 7. Event Logs & Fault History ---------
    // Dummy data: 64 bytes of zeros; store 64+1 bytes.
    {
        const size_t data_len = 64;
        uint8_t dummy[data_len];
        memset(dummy, 0, sizeof(dummy));
        uint8_t crc = calculate_crc8(dummy, data_len);
        uint8_t stored[data_len + 1];
        CAT24C512_ReadBuffer(EEPROM_EVENT_LOG_START, stored, data_len + 1);
        if ((memcmp(stored, dummy, data_len) == 0) && (stored[data_len] == crc)) {
            printf("Event logs unchanged. Skipping write.\n");
        } else {
            uint8_t write_buf[data_len + 1];
            memcpy(write_buf, dummy, data_len);
            write_buf[data_len] = crc;
            if (CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START, write_buf, data_len + 1) != 0)
                printf("Failed to write event logs.\n");
            else
                printf("Event logs written.\n");
        }
    }
    
    // --------- 8. User Preferences ---------
    // Dummy data: 64 bytes of zeros; store 64+1 bytes.
    {
        const size_t data_len = 64;
        uint8_t dummy[data_len];
        memset(dummy, 0, sizeof(dummy));
        uint8_t crc = calculate_crc8(dummy, data_len);
        uint8_t stored[data_len + 1];
        CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, stored, data_len + 1);
        if ((memcmp(stored, dummy, data_len) == 0) && (stored[data_len] == crc)) {
            printf("User preferences unchanged. Skipping write.\n");
        } else {
            uint8_t write_buf[data_len + 1];
            memcpy(write_buf, dummy, data_len);
            write_buf[data_len] = crc;
            if (CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, write_buf, data_len + 1) != 0)
                printf("Failed to write user preferences.\n");
            else
                printf("User preferences written.\n");
        }
    }
    
    printf("Dummy EEPROM content update complete.\n");
}