/**
 * @file src/drivers/MCP23017_driver.h
 * @author DvidMakesThings - David Sipos
 *
 * @defgroup drivers Drivers
 * @brief HAL drivers for the Energis PDU firmware.
 * @{
 * 
 * @defgroup driver1 1. MCP23017 Driver
 * @ingroup drivers
 * @brief Header file for MCP23017 I2C GPIO expander driver
 * @{
 *
 * @version 1.0.0
 * @date 2025-11-06
 * @details
 * All functions accept the I2C address via the registered device context.
 * Uses FreeRTOS mutex for thread-safety, shadowed OLAT writes to avoid
 * torn RMW, conservative I2C retries with small delays, and optional HW reset.
 *
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#ifndef MCP23017_DRIVER_H
#define MCP23017_DRIVER_H

#include "../CONFIG.h"

/* ========= MCP23017 register map (BANK=0, sequential addresses) ========= */
enum {
    MCP23017_IODIRA = 0x00,
    MCP23017_IODIRB = 0x01,
    MCP23017_IPOLA = 0x02,
    MCP23017_IPOLB = 0x03,
    MCP23017_GPINTENA = 0x04,
    MCP23017_GPINTENB = 0x05,
    MCP23017_DEFVALA = 0x06,
    MCP23017_DEFVALB = 0x07,
    MCP23017_INTCONA = 0x08,
    MCP23017_INTCONB = 0x09,
    MCP23017_IOCON = 0x0A,
    MCP23017_IOCONB = 0x0B,
    MCP23017_GPPUA = 0x0C,
    MCP23017_GPPUB = 0x0D,
    MCP23017_INTFA = 0x0E,
    MCP23017_INTFB = 0x0F,
    MCP23017_INTCAPA = 0x10,
    MCP23017_INTCAPB = 0x11,
    MCP23017_GPIOA = 0x12,
    MCP23017_GPIOB = 0x13,
    MCP23017_OLATA = 0x14,
    MCP23017_OLATB = 0x15
};

/**
 * @brief Opaque device context for one MCP23017 instance.
 *
 * Holds I2C bus pointer, address, optional reset GPIO, shadow OLAT copies,
 * and a mutex for race-free multi-task access.
 */
typedef struct {
    i2c_inst_t *i2c;         /* Pico SDK I2C instance (i2c0/i2c1). */
    uint8_t addr;            /* 7-bit address 0x20..0x27. */
    int8_t rst_gpio;         /* Optional reset GPIO, -1 if none. */
    volatile uint8_t olat_a; /* Software shadow of OLATA. */
    volatile uint8_t olat_b; /* Software shadow of OLATB. */
    SemaphoreHandle_t mutex; /* FreeRTOS mutex. */
    bool inited;             /* True after mcp_init(). */
} mcp23017_t;

/**
 * @brief Register (or fetch) a device context for a given I2C address.
 *
 * If the address is already registered, returns the existing context.
 * Otherwise creates a new one, creates a mutex, stores bus/address/reset.
 *
 * @param i2c      I2C instance pointer.
 * @param addr     7-bit MCP23017 address.
 * @param rst_gpio Optional reset GPIO (active low). Use -1 if not wired.
 * @return mcp23017_t* Pointer to device context, or NULL on error.
 */
mcp23017_t *mcp_register(i2c_inst_t *i2c, uint8_t addr, int8_t rst_gpio);

/**
 * @brief Initialize a device to a deterministic, safe state.
 *
 * BANK=0, SEQOP=1, all pins outputs by default, no pullups,
 * seeds shadow from OLAT, then writes shadows back to hardware.
 *
 * @param dev Pointer returned by mcp_register().
 */
void mcp_init(mcp23017_t *dev);

/**
 * @brief Robust register write with retries (mutex-guarded).
 *
 * Keeps OLAT shadows in sync if writing OLATA/OLATB.
 *
 * @param dev   Device context.
 * @param reg   Register address.
 * @param value Value to write.
 * @return true on success, false on I2C failure.
 */
bool mcp_write_reg(mcp23017_t *dev, uint8_t reg, uint8_t value);

/**
 * @brief Robust register read with retries (mutex-guarded).
 *
 * @param dev Device context.
 * @param reg Register to read.
 * @param out Destination pointer for read value.
 * @return true on success, false on I2C failure or bad args.
 */
bool mcp_read_reg(mcp23017_t *dev, uint8_t reg, uint8_t *out);

/**
 * @brief Configure one pin as input(1) or output(0).
 *
 * Read-modify-write of IODIRx under mutex.
 *
 * @param dev       Device context.
 * @param pin       0–15 (0..7=A, 8..15=B).
 * @param direction 1=input, 0=output.
 */
void mcp_set_direction(mcp23017_t *dev, uint8_t pin, uint8_t direction);

/**
 * @brief Write a logic level to a pin (uses shadow + whole-port write).
 *
 * Prevents torn updates when multiple tasks touch the same port.
 *
 * @param dev   Device context.
 * @param pin   0–15 (0..7=A, 8..15=B).
 * @param value 0=low, 1=high.
 */
void mcp_write_pin(mcp23017_t *dev, uint8_t pin, uint8_t value);

/**
 * @brief Read a pin level via GPIOx.
 *
 * @param dev Device context.
 * @param pin 0–15 (0..7=A, 8..15=B).
 * @return 0=low, 1=high.
 */
uint8_t mcp_read_pin(mcp23017_t *dev, uint8_t pin);

/**
 * @brief Atomically set/clear a masked group on a port from shadows.
 *
 * @param dev       Device context.
 * @param port_ab   0=Port A, 1=Port B.
 * @param mask      Bits to affect.
 * @param value_bits Values for those bits (only masked bits applied).
 */
void mcp_write_mask(mcp23017_t *dev, uint8_t port_ab, uint8_t mask, uint8_t value_bits);

/**
 * @brief Re-sync software shadows from hardware OLATx registers.
 *
 * @param dev Device context.
 * @return None
 */
void mcp_resync_from_hw(mcp23017_t *dev);

/** 
 * @brief Initialize all MCP23017 devices used in the system.
 * 
 * @param None
 * @return None
 * @note ===== Board binding (uses CONFIG.h macros) =====
   Required in CONFIG.h:
     - I2C_SPEED
     - MCP23017_DISPLAY_I2C (e.g., i2c0), MCP23017_RELAY_I2C (e.g., i2c1), MCP23017_SELECTION_I2C
     - MCP_DISPLAY_ADDR, MCP_RELAY_ADDR, MCP_SELECTION_ADDR
     - Optional pins (only if you want this driver to set the pinmux):
         I2C0_SDA, I2C0_SCL, I2C1_SDA, I2C1_SCL
     - Optional resets:
         MCP_MB_RST, MCP_DP_RST (or leave undefined -> -1)
*/
void MCP2017_Init(void);
mcp23017_t *mcp_relay(void);
mcp23017_t *mcp_display(void);
mcp23017_t *mcp_selection(void);

/**
 * @brief Sets one channel on the RELAY MCP and mirrors it to the DISPLAY MCP.
 * ch: 0..7, value: 0=off, 1=on. Returns true on success (relay MCP present). 
 * 
 * @param ch Channel index 0..7
 * @param value 0=off, 1=on
 * @return true on success
 * 
*/
bool mcp_set_channel_state(uint8_t ch, uint8_t value);

/**
 * @brief Gets one channel state from the RELAY MCP.
 * ch: 0..7. Returns true if ON, false if OFF or error.
 * 
 * @param ch Channel index 0..7
 * @return true if relay is ON, false if OFF or error
 */
bool mcp_get_channel_state(uint8_t ch);

/**
 * @brief Set or clear the FAULT LED on the DISPLAY MCP.
 * state: true=ON, false=OFF. Returns true on success (display MCP present).
 * 
 * @param state true=ON, false=OFF
 * @return true on success
 */
bool setError(bool state);

/**
 * @brief Set or clear the POWER GOOD indicator on the DISPLAY MCP.
 * state: true=ON, false=OFF. Returns true on success (display MCP present).
 * 
 * @param state true=ON, false=OFF
 * @return true on success
 */
bool setPowerGood(bool state);

/**
 * @brief Set or clear the NETWORK LINK indicator on the DISPLAY MCP.
 * state: true=ON, false=OFF. Returns true on success (display MCP present).
 * 
 * @param state true=ON, false=OFF
 * @return true on success
 */
bool setNetworkLink(bool state);

#endif /* MCP23017_DRIVER_H */

/** @} */