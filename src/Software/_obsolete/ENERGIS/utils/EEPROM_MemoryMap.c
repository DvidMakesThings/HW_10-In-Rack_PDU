/**
 * @file EEPROM_MemoryMap.c
 * @author Dvid
 * @defgroup utils Utils
 * @brief Utility modules for the Energis PDU.
 * @{
 *
 * @defgroup util1 1. EEPROM Memory Map
 * @ingroup utils
 * @brief Implementation of the EEPROM Memory Map functions.
 * @{
 * @version 1.5
 * @date 2025-11-02
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "utils/EEPROM_MemoryMap.h"
#include "drivers/CAT24C512_driver.h"
#include <stdio.h>
#include <string.h>

/* =====================  Local helpers  ================================ */
/**
 * @brief Calculates the CRC-8 checksum of the given data.
 * @param data buffer
 * @param len length
 * @return CRC-8 checksum value.
 */
static uint8_t calculate_crc8(const uint8_t *data, size_t len);

/* =====================  Default user prefs (local)  =================== */
static const userPrefInfo DEFAULT_USER_PREFS = {
    .device_name = DEFAULT_NAME, .location = DEFAULT_LOCATION, .temp_unit = 0};

/* ====================================================================== */
/*                              EEPROM API                                */
/* ====================================================================== */

/**
 * @brief Erase the entire EEPROM (write 0xFF).
 * @return 0 on success, -1 on error.
 */
int EEPROM_EraseAll(void) {
    uint8_t blank[32];
    memset(blank, 0xFF, sizeof(blank));

    for (uint16_t addr = 0; addr < 0x8000; addr += sizeof(blank)) {
        if (CAT24C512_WriteBuffer(addr, blank, sizeof(blank)) != 0) {
            return -1;
        }
    }
    return 0;
}

/* =====================  System Info & Calibration  ==================== */

/**
 * @brief Write system info.
 * @param data buffer
 * @param len <= EEPROM_SYS_INFO_SIZE
 * @return 0/ -1
 */
int EEPROM_WriteSystemInfo(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_SYS_INFO_START, data, len);
}

/**
 * @brief Read system info.
 * @param data out buffer
 * @param len <= EEPROM_SYS_INFO_SIZE
 * @return 0/ -1
 */
int EEPROM_ReadSystemInfo(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_SYS_INFO_START, data, len);
    return 0;
}

/* =====================  Factory Defaults  ============================= */

/**
 * @brief Write factory defaults to all sections.
 * @return 0 on success, -1 if any write failed.
 */
int EEPROM_WriteFactoryDefaults(void) {
    int status = 0;

    /* Serial Number + SWVERSION */
    char sys_info_buf[64];
    size_t sn_len = strlen(DEFAULT_SN) + 1;
    size_t swv_len = strlen(SWVERSION) + 1;
    memset(sys_info_buf, 0, sizeof(sys_info_buf));
    memcpy(sys_info_buf, DEFAULT_SN, sn_len);
    memcpy(sys_info_buf + sn_len, SWVERSION, swv_len);
    status |= EEPROM_WriteSystemInfo((const uint8_t *)sys_info_buf, sn_len + swv_len);
    ECHO("Serial Number and SWVERSION written to EEPROM.\n");

    /* Relay Status (all OFF) */
    status |= EEPROM_WriteUserOutput(DEFAULT_RELAY_STATUS, sizeof(DEFAULT_RELAY_STATUS));
    ECHO("Relay Status written to EEPROM.\n");

    /* Network Configuration (with CRC) */
    status |= EEPROM_WriteUserNetworkWithChecksum(&DEFAULT_NETWORK);
    ECHO("Network Configuration written to EEPROM.\n");

    /* Sensor Calibration (generic blob) — zeroed */
    hlw_calib_data_t zero_cal;
    memset(&zero_cal, 0xFF, sizeof(zero_cal)); /* mark as not calibrated */
    for (int i = 0; i < 8; i++) {
        zero_cal.channels[i].voltage_factor = HLW8032_VF;
        zero_cal.channels[i].current_factor = HLW8032_CF;
        zero_cal.channels[i].r1_actual = 1880000.0f;
        zero_cal.channels[i].r2_actual = 1000.0f;
        zero_cal.channels[i].shunt_actual = 0.001f;
    }
    status |= EEPROM_WriteSensorCalibration((const uint8_t *)&zero_cal, sizeof(zero_cal));
    ECHO("Sensor Calibration (generic) written to EEPROM.\n");

    /* Energy Monitoring Data */
    status |= EEPROM_WriteEnergyMonitoring(DEFAULT_ENERGY_DATA, sizeof(DEFAULT_ENERGY_DATA));
    ECHO("Energy Monitoring Data written to EEPROM.\n");

    /* Event Logs */
    status |= EEPROM_WriteEventLogs(DEFAULT_LOG_DATA, sizeof(DEFAULT_LOG_DATA));
    ECHO("Event Logs written to EEPROM.\n");

    /* User Preferences (default name/location) */
    status |= EEPROM_WriteDefaultNameLocation();
    ECHO("User Preferences written to EEPROM.\n");

    if (status == 0) {
        ECHO("EEPROM factory defaults written successfully.\n");
    } else {
        setError(true);
        ERROR_PRINT("EEPROM factory defaults write encountered errors.\n");
    }
    return status;
}

/**
 * @brief Validate key sections in EEPROM; log errors if mismatch.
 * @return 0 (we log and continue; restore handled elsewhere if desired).
 */
int EEPROM_ReadFactoryDefaults(void) {
    /* Serial Number check */
    char stored_sn[strlen(DEFAULT_SN) + 1];
    EEPROM_ReadSystemInfo((uint8_t *)stored_sn, strlen(DEFAULT_SN) + 1);
    if (memcmp(stored_sn, DEFAULT_SN, strlen(DEFAULT_SN)) != 0) {
        setError(true);
        ERROR_PRINT("Invalid Serial Number detected in EEPROM.\n");
    }

    /* Relay Status */
    uint8_t stored_relay_status[sizeof(DEFAULT_RELAY_STATUS)];
    EEPROM_ReadUserOutput(stored_relay_status, sizeof(DEFAULT_RELAY_STATUS));
    if (memcmp(stored_relay_status, DEFAULT_RELAY_STATUS, sizeof(DEFAULT_RELAY_STATUS)) != 0) {
        setError(true);
        ERROR_PRINT("Invalid Relay Status detected in EEPROM.\n");
    }

    /* Network (CRC verified) */
    networkInfo stored_network;
    if (EEPROM_ReadUserNetworkWithChecksum(&stored_network) != 0) {
        setError(true);
        ERROR_PRINT("Invalid Network Configuration detected in EEPROM.\n");
    }

    /* Sensor Calibration — presence check (no CRC here) */
    hlw_calib_data_t tmp;
    if (EEPROM_ReadSensorCalibration((uint8_t *)&tmp, sizeof(tmp)) != 0) {
        setError(true);
        ERROR_PRINT("Sensor Calibration missing/invalid.\n");
    }

    /* Energy, Logs, User Prefs — perform simple presence reads */
    uint8_t buf[64];
    EEPROM_ReadEnergyMonitoring(buf, sizeof(buf));
    EEPROM_ReadEventLogs(buf, sizeof(buf));
    EEPROM_ReadUserPreferences(buf, sizeof(buf));

    INFO_PRINT("EEPROM content checked.\n\n");
    return 0;
}

/* =====================  User Output Configuration  ==================== */

/**
 * @brief Write user output configuration.
 * @param data buffer
 * @param len <= EEPROM_USER_OUTPUT_SIZE
 * @return 0/-1
 */
int EEPROM_WriteUserOutput(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_OUTPUT_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_OUTPUT_START, data, len);
}

/**
 * @brief Read user output configuration.
 * @param data out buffer
 * @param len <= EEPROM_USER_OUTPUT_SIZE
 * @return 0/-1
 */
int EEPROM_ReadUserOutput(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_OUTPUT_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_OUTPUT_START, data, len);
    return 0;
}

/* =====================  User Network Settings  ======================== */

/**
 * @brief Write user network settings (raw).
 * @param data buffer
 * @param len <= EEPROM_USER_NETWORK_SIZE
 * @return 0/-1
 */
int EEPROM_WriteUserNetwork(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_NETWORK_START, data, len);
}

/**
 * @brief Read user network settings (raw).
 * @param data out buffer
 * @param len <= EEPROM_USER_NETWORK_SIZE
 * @return 0/-1
 */
int EEPROM_ReadUserNetwork(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_NETWORK_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_NETWORK_START, data, len);
    return 0;
}

/* =====================  Sensor Calibration (generic)  ================= */

/**
 * @brief Write Sensor Calibration blob (generic).
 * @param data pointer to blob (e.g., hlw_calib_data_t)
 * @param len  blob size (<= EEPROM_SENSOR_CAL_SIZE)
 * @return 0/-1
 */
int EEPROM_WriteSensorCalibration(const uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_SENSOR_CAL_START, data, len);
}

/**
 * @brief Round a float to 5 fractional digits.
 * @param v Input value
 * @return Value rounded to 5 digits after the decimal point.
 */
static inline float eep_round5(float v) {
    const float s = 100000.0f;
    if (v >= 0.0f) {
        uint32_t u = (uint32_t)(v * s + 0.5f);
        return (float)u / s;
    } else {
        int32_t i = (int32_t)(v * s - 0.5f);
        return (float)i / s;
    }
}

/**
 * @brief Round a float to 6 fractional digits.
 * @param v Input value
 * @return Value rounded to 6 digits after the decimal point.
 */
static inline float eep_round6(float v) {
    const float s = 1000000.0f;
    if (v >= 0.0f) {
        uint32_t u = (uint32_t)(v * s + 0.5f);
        return (float)u / s;
    } else {
        int32_t i = (int32_t)(v * s - 0.5f);
        return (float)i / s;
    }
}

/**
 * @brief Write a single channel's calibration record to EEPROM (fixed 32-byte slot).
 *        Floats are stored in LE in the order: vf, cf, v_off, c_off, r1, r2, shunt.
 *        vf/cf/offsets are rounded to 5 digits; shunt to 6 digits.
 */
int EEPROM_WriteSensorCalibrationForChannel(uint8_t ch, const hlw_calib_t *in) {
    if (ch >= 8u || in == NULL)
        return -1;

    enum { SLOT_LEN = 32 };
    uint8_t raw[SLOT_LEN];
    memset(raw, 0xFF, sizeof(raw));

    float vf = (in->voltage_factor > 0.0f) ? eep_round5(in->voltage_factor) : HLW8032_VF;
    float cf = (in->current_factor > 0.0f) ? eep_round5(in->current_factor) : HLW8032_CF;
    float v_off = (in->voltage_offset > -1000.0f && in->voltage_offset < 1000.0f)
                      ? eep_round5(in->voltage_offset)
                      : 0.0f;
    float c_off = (in->current_offset > -1000.0f && in->current_offset < 1000.0f)
                      ? eep_round5(in->current_offset)
                      : 0.0f;
    float r1 = (in->r1_actual > 0.0f) ? in->r1_actual : 1880000.0f;
    float r2 = (in->r2_actual > 0.0f) ? in->r2_actual : 1000.0f;
    float shunt = (in->shunt_actual > 0.0f) ? eep_round6(in->shunt_actual) : 0.001f;

    raw[0] = (in->calibrated == 0xCA) ? 0xCA : 0xFF;
    raw[1] = (in->zero_calibrated == 0xCA) ? 0xCA : 0xFF;
    raw[2] = 0x00;
    raw[3] = 0x00;

    memcpy(&raw[4], &vf, 4);
    memcpy(&raw[8], &cf, 4);
    memcpy(&raw[12], &v_off, 4);
    memcpy(&raw[16], &c_off, 4);
    memcpy(&raw[20], &r1, 4);
    memcpy(&raw[24], &r2, 4);
    memcpy(&raw[28], &shunt, 4);

    const uint16_t addr = (uint16_t)(EEPROM_SENSOR_CAL_START + (uint32_t)ch * SLOT_LEN);
    return CAT24C512_WriteBuffer(addr, raw, SLOT_LEN);
}

/**
 * @brief Read Sensor Calibration blob (generic).
 * @param data out buffer
 * @param len  size to read (<= EEPROM_SENSOR_CAL_SIZE)
 * @return 0/-1
 */
int EEPROM_ReadSensorCalibration(uint8_t *data, size_t len) {
    if (len > EEPROM_SENSOR_CAL_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_SENSOR_CAL_START, data, len);
    return 0;
}

/**
 * @brief Read a single channel's calibration record from EEPROM (fixed 32-byte slot).
 *        Slot layout (LE):
 *          [0]  calibrated (0xCA or 0xFF)
 *          [1]  zero_calibrated (0xCA or 0xFF)
 *          [2:3] reserved (0)
 *          [4..31] floats: vf, cf, v_off, c_off, r1, r2, shunt
 */
int EEPROM_ReadSensorCalibrationForChannel(uint8_t ch, hlw_calib_t *out) {
    if (ch >= 8u || out == NULL)
        return -1;

    enum { SLOT_LEN = 32 };
    uint8_t raw[SLOT_LEN];
    const uint16_t addr = (uint16_t)(EEPROM_SENSOR_CAL_START + (uint32_t)ch * SLOT_LEN);
    CAT24C512_ReadBuffer(addr, raw, SLOT_LEN);

    out->calibrated = (raw[0] == 0xCA) ? 0xCA : 0xFF;
    out->zero_calibrated = (raw[1] == 0xCA) ? 0xCA : 0xFF;

    float vf, cf, v_off, c_off, r1, r2, shunt;
    memcpy(&vf, &raw[4], 4);
    memcpy(&cf, &raw[8], 4);
    memcpy(&v_off, &raw[12], 4);
    memcpy(&c_off, &raw[16], 4);
    memcpy(&r1, &raw[20], 4);
    memcpy(&r2, &raw[24], 4);
    memcpy(&shunt, &raw[28], 4);

    if (!(vf > 0.0f && vf < 1e6f))
        vf = HLW8032_VF;
    if (!(cf > 0.0f && cf < 1e6f))
        cf = HLW8032_CF;
    if (!(v_off > -1000.0f && v_off < 1000.0f))
        v_off = 0.0f;
    if (!(c_off > -1000.0f && c_off < 1000.0f))
        c_off = 0.0f;
    if (!(r1 > 0.0f && r1 < 1e9f))
        r1 = 1880000.0f;
    if (!(r2 > 0.0f && r2 < 1e9f))
        r2 = 1000.0f;
    if (!(shunt > 0.0f && shunt < 1.0f))
        shunt = 0.001f;

    out->voltage_factor = vf;
    out->current_factor = cf;
    out->voltage_offset = v_off;
    out->current_offset = c_off;
    out->r1_actual = r1;
    out->r2_actual = r2;
    out->shunt_actual = shunt;
    return 0;
}

/* =====================  Energy Monitoring  ============================ */

/**
 * @brief Write energy monitoring data.
 * @param data buffer
 * @param len <= EEPROM_ENERGY_MON_SIZE
 * @return 0/-1
 */
int EEPROM_WriteEnergyMonitoring(const uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START, data, len);
}

/**
 * @brief Read energy monitoring data.
 * @param data out buffer
 * @param len <= EEPROM_ENERGY_MON_SIZE
 * @return 0/-1
 */
int EEPROM_ReadEnergyMonitoring(uint8_t *data, size_t len) {
    if (len > EEPROM_ENERGY_MON_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_ENERGY_MON_START, data, len);
    return 0;
}

/* =====================  Event Logs  ================================== */

/**
 * @brief Write event logs.
 * @param data buffer
 * @param len <= EEPROM_EVENT_LOG_SIZE
 * @return 0/-1
 */
int EEPROM_WriteEventLogs(const uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START, data, len);
}

/**
 * @brief Read event logs.
 * @param data out buffer
 * @param len <= EEPROM_EVENT_LOG_SIZE
 * @return 0/-1
 */
int EEPROM_ReadEventLogs(uint8_t *data, size_t len) {
    if (len > EEPROM_EVENT_LOG_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_EVENT_LOG_START, data, len);
    return 0;
}

/* =====================  User Preferences  ============================= */

/**
 * @brief Write user preferences (raw).
 * @param data buffer
 * @param len <= EEPROM_USER_PREF_SIZE
 * @return 0/-1
 */
int EEPROM_WriteUserPreferences(const uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE)
        return -1;
    return CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, data, len);
}

/**
 * @brief Read user preferences (raw).
 * @param data out buffer
 * @param len <= EEPROM_USER_PREF_SIZE
 * @return 0/-1
 */
int EEPROM_ReadUserPreferences(uint8_t *data, size_t len) {
    if (len > EEPROM_USER_PREF_SIZE)
        return -1;
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, data, len);
    return 0;
}

/* =====================  Checksums & Helpers  ========================== */

/**
 * @brief CRC-8 over arbitrary data.
 * @param data buffer
 * @param len length
 * @return CRC byte
 */
static uint8_t calculate_crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x07);
            else
                crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief Write system info with CRC-8 at the end.
 * @param data payload (<= SIZE-1)
 * @param len length
 * @return 0/-1
 */
int EEPROM_WriteSystemInfoWithChecksum(const uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1)
        return -1;
    uint8_t buffer[EEPROM_SYS_INFO_SIZE];
    memcpy(buffer, data, len);
    buffer[len] = calculate_crc8(data, len);
    return CAT24C512_WriteBuffer(EEPROM_SYS_INFO_START, buffer, len + 1);
}

/**
 * @brief Read system info with CRC-8 verification.
 * @param data out payload (<= SIZE-1)
 * @param len length to read
 * @return 0 if CRC OK, -1 on mismatch.
 */
int EEPROM_ReadSystemInfoWithChecksum(uint8_t *data, size_t len) {
    if (len > EEPROM_SYS_INFO_SIZE - 1)
        return -1;
    uint8_t buffer[EEPROM_SYS_INFO_SIZE];
    CAT24C512_ReadBuffer(EEPROM_SYS_INFO_START, buffer, len + 1);
    uint8_t crc = calculate_crc8(buffer, len);
    if (crc != buffer[len]) {
        return -1;
    }
    memcpy(data, buffer, len);
    return 0;
}

/**
 * @brief Write network config with CRC-8.
 * @param net_info pointer to struct
 * @return 0/-1
 */
int EEPROM_WriteUserNetworkWithChecksum(const networkInfo *net_info) {
    uint8_t buffer[24]; /* 16 (IP,GW,SN,MAC) + 4 (DNS) + 1 (DHCP) + 1 (CRC) */

    memcpy(&buffer[0], net_info->ip, 4);
    memcpy(&buffer[4], net_info->gw, 4);
    memcpy(&buffer[8], net_info->sn, 4);
    memcpy(&buffer[12], net_info->mac, 6);
    memcpy(&buffer[18], net_info->dns, 4);
    buffer[22] = net_info->dhcp;

    buffer[23] = calculate_crc8(buffer, 23);
    return CAT24C512_WriteBuffer(EEPROM_USER_NETWORK_START, buffer, 24);
}

/**
 * @brief Read network config with CRC-8.
 * @param net_info out
 * @return 0 if CRC OK
 */
int EEPROM_ReadUserNetworkWithChecksum(networkInfo *net_info) {
    uint8_t buffer[24];
    CAT24C512_ReadBuffer(EEPROM_USER_NETWORK_START, buffer, 24);

    if (calculate_crc8(buffer, 23) == buffer[23]) {
        memcpy(net_info->ip, &buffer[0], 4);
        memcpy(net_info->gw, &buffer[4], 4);
        memcpy(net_info->sn, &buffer[8], 4);
        memcpy(net_info->mac, &buffer[12], 6);
        memcpy(net_info->dns, &buffer[18], 4);
        net_info->dhcp = buffer[22];
        return 0;
    }
    return -1;
}

/* =====================  Wear leveling samples (kept)  ================= */

/**
 * @brief Read current write pointer for energy records.
 * @return pointer
 */
static uint16_t read_energy_pointer(void) {
    uint16_t ptr = ((uint16_t)CAT24C512_ReadByte(EEPROM_ENERGY_MON_START) << 8) |
                   CAT24C512_ReadByte(EEPROM_ENERGY_MON_START + 1);
    return ptr;
}

/**
 * @brief Update write pointer for energy records.
 * @param ptr new pointer
 */
static void write_energy_pointer(uint16_t ptr) {
    CAT24C512_WriteByte(EEPROM_ENERGY_MON_START, ptr >> 8);
    CAT24C512_WriteByte(EEPROM_ENERGY_MON_START + 1, ptr & 0xFF);
}

/**
 * @brief Append an energy monitoring record.
 * @param data ENERGY_RECORD_SIZE bytes
 * @return 0/-1
 */
int EEPROM_AppendEnergyRecord(const uint8_t *data) {
    uint16_t ptr = read_energy_pointer();
    uint16_t effective = EEPROM_ENERGY_MON_SIZE - ENERGY_MON_POINTER_SIZE;
    if (ptr + ENERGY_RECORD_SIZE > effective) {
        ptr = 0;
    }
    int res = CAT24C512_WriteBuffer(EEPROM_ENERGY_MON_START + ENERGY_MON_POINTER_SIZE + ptr, data,
                                    ENERGY_RECORD_SIZE);
    if (res != 0)
        return res;
    ptr += ENERGY_RECORD_SIZE;
    write_energy_pointer(ptr);
    return 0;
}

/**
 * @brief Read current append-pointer for event logs.
 * @return pointer
 */
static uint16_t read_event_log_pointer(void) {
    uint16_t ptr = ((uint16_t)CAT24C512_ReadByte(EEPROM_EVENT_LOG_START) << 8) |
                   CAT24C512_ReadByte(EEPROM_EVENT_LOG_START + 1);
    return ptr;
}

/**
 * @brief Update append-pointer for event logs.
 * @param ptr new pointer
 */
static void write_event_log_pointer(uint16_t ptr) {
    CAT24C512_WriteByte(EEPROM_EVENT_LOG_START, ptr >> 8);
    CAT24C512_WriteByte(EEPROM_EVENT_LOG_START + 1, ptr & 0xFF);
}

/**
 * @brief Append an event log entry.
 * @param entry EVENT_LOG_ENTRY_SIZE bytes
 * @return 0/-1
 */
int EEPROM_AppendEventLog(const uint8_t *entry) {
    uint16_t ptr = read_event_log_pointer();
    uint16_t effective = EEPROM_EVENT_LOG_SIZE - EVENT_LOG_POINTER_SIZE;
    if (ptr + EVENT_LOG_ENTRY_SIZE > effective) {
        ptr = 0;
    }
    int res = CAT24C512_WriteBuffer(EEPROM_EVENT_LOG_START + EVENT_LOG_POINTER_SIZE + ptr, entry,
                                    EVENT_LOG_ENTRY_SIZE);
    if (res != 0)
        return res;
    ptr += EVENT_LOG_ENTRY_SIZE;
    write_event_log_pointer(ptr);
    return 0;
}

/* =====================  User Pref helpers  ============================ */

/**
 * @brief Write default device name/location with CRC.
 * @return 0/-1
 */
int EEPROM_WriteDefaultNameLocation(void) {
    /* 32 name + 32 location + 1 temp_unit + 1 CRC = 66 */
    uint8_t buffer[66];
    memset(buffer, 0x00, sizeof(buffer));

    strncpy((char *)&buffer[0], DEFAULT_NAME, 31);
    buffer[31] = '\0';
    strncpy((char *)&buffer[32], DEFAULT_LOCATION, 31);
    buffer[63] = '\0';

    buffer[64] = 0x00; /* Celsius */

    buffer[65] = calculate_crc8(buffer, 65);

    int res = CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, buffer, 64);
    res |= CAT24C512_WriteBuffer(EEPROM_USER_PREF_START + 64, &buffer[64], 2);
    return res;
}

/**
 * @brief Write user preferences with CRC.
 * @param prefs struct
 * @return 0/-1
 */
int EEPROM_WriteUserPrefsWithChecksum(const userPrefInfo *prefs) {
    if (!prefs)
        return -1;

    uint8_t record[66];
    memset(record, 0x00, sizeof(record));

    memcpy(&record[0], prefs->device_name, 32);
    memcpy(&record[32], prefs->location, 32);
    record[64] = prefs->temp_unit;

    record[65] = calculate_crc8(&record[0], 65);

    int res = CAT24C512_WriteBuffer(EEPROM_USER_PREF_START, record, 64);
    res |= CAT24C512_WriteBuffer(EEPROM_USER_PREF_START + 64, &record[64], 2);
    return res;
}

/**
 * @brief Read user preferences with CRC verification.
 * @param prefs out
 * @return 0 if CRC OK
 */
int EEPROM_ReadUserPrefsWithChecksum(userPrefInfo *prefs) {
    if (!prefs)
        return -1;

    uint8_t record[66];
    CAT24C512_ReadBuffer(EEPROM_USER_PREF_START, record, 66);

    if (calculate_crc8(&record[0], 65) != record[65])
        return -1;

    memcpy(prefs->device_name, &record[0], 32);
    prefs->device_name[31] = '\0';
    memcpy(prefs->location, &record[32], 32);
    prefs->location[31] = '\0';
    prefs->temp_unit = record[64];

    return 0;
}

/**
 * @brief Load user preferences, fallback to defaults if CRC fails.
 * @return prefs struct.
 */
userPrefInfo LoadUserPreferences(void) {
    userPrefInfo prefs;
    if (EEPROM_ReadUserPrefsWithChecksum(&prefs) == 0) {
        INFO_PRINT("Loaded user prefs\n");
    } else {
        setError(true);
        ERROR_PRINT("No saved prefs, using defaults\n");
        prefs = DEFAULT_USER_PREFS;
    }
    return prefs;
}

/**
 * @brief Load user network configuration, fallback to defaults if CRC fails.
 * @return networkInfo
 */
networkInfo LoadUserNetworkConfig(void) {
    networkInfo net_info;
    if (EEPROM_ReadUserNetworkWithChecksum(&net_info) == 0) {
        INFO_PRINT("Loaded network config from EEPROM.\n");
    } else {
        setError(true);
        ERROR_PRINT("EEPROM network config invalid. Using defaults.\n");
        net_info = DEFAULT_NETWORK;
    }
    return net_info;
}

/* =====================  Channel labels (per-channel)  ================= */

/**
 * @brief Compute EEPROM address of a channel's label slot.
 * @param channel_index 0..N-1
 * @return 16-bit address
 */
static inline uint16_t _LabelSlotAddr(uint8_t channel_index) {
    return (uint16_t)(EEPROM_CH_LABEL_START + (uint32_t)channel_index * EEPROM_CH_LABEL_SLOT);
}

/**
 * @brief Write a single channel label.
 * @param channel_index 0..ENERGIS_NUM_CHANNELS-1
 * @param label UTF-8 string
 * @return 0/-1
 */
int EEPROM_WriteChannelLabel(uint8_t channel_index, const char *label) {
    if (channel_index >= ENERGIS_NUM_CHANNELS || label == NULL)
        return -1;

    uint8_t buf[EEPROM_CH_LABEL_SLOT];
    size_t maxcpy = EEPROM_CH_LABEL_SLOT - 1u;

    size_t n = 0u;
    for (; n < maxcpy && label[n] != '\0'; ++n)
        buf[n] = (uint8_t)label[n];

    buf[n++] = 0x00;
    for (; n < EEPROM_CH_LABEL_SLOT; ++n)
        buf[n] = 0x00;

    return CAT24C512_WriteBuffer(_LabelSlotAddr(channel_index), buf, EEPROM_CH_LABEL_SLOT);
}

/**
 * @brief Read a single channel label.
 * @param channel_index 0..ENERGIS_NUM_CHANNELS-1
 * @param out buffer
 * @param out_len bytes
 * @return 0/-1
 */
int EEPROM_ReadChannelLabel(uint8_t channel_index, char *out, size_t out_len) {
    if (channel_index >= ENERGIS_NUM_CHANNELS || out == NULL || out_len == 0u)
        return -1;

    uint8_t buf[EEPROM_CH_LABEL_SLOT];
    CAT24C512_ReadBuffer(_LabelSlotAddr(channel_index), buf, EEPROM_CH_LABEL_SLOT);

    size_t i = 0u;
    while (i + 1u < out_len && i < EEPROM_CH_LABEL_SLOT && buf[i] != 0x00) {
        out[i] = (char)buf[i];
        ++i;
    }
    out[i] = '\0';
    return 0;
}

/**
 * @brief Clear a single channel label (set slot to 0x00).
 * @param channel_index 0..ENERGIS_NUM_CHANNELS-1
 * @return 0/-1
 */
int EEPROM_ClearChannelLabel(uint8_t channel_index) {
    if (channel_index >= ENERGIS_NUM_CHANNELS)
        return -1;

    uint8_t zero[32];
    memset(zero, 0x00, sizeof(zero));

    for (uint16_t addr = 0; addr < EEPROM_CH_LABEL_SLOT; addr += sizeof(zero)) {
        if (CAT24C512_WriteBuffer(_LabelSlotAddr(channel_index) + addr, zero, sizeof(zero)) != 0)
            return -1;
    }
    return 0;
}

/**
 * @brief Clear all channel labels.
 * @return 0/-1
 */
int EEPROM_ClearAllChannelLabels(void) {
    for (uint8_t ch = 0; ch < ENERGIS_NUM_CHANNELS; ++ch)
        if (EEPROM_ClearChannelLabel(ch) != 0)
            return -1;
    return 0;
}

/** @} @} */
