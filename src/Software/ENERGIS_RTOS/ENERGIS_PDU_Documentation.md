# ENERGIS - 8 Channel Managed PDU for 10 Inch Racks
## Complete Technical Documentation

**Version:** 1.1.0 (Firmware) / 1.1.0 (Hardware)
**Author:** DvidMakesThings - David Sipos
**Project:** ENERGIS - The Managed PDU Project for 10-Inch Rack
**GitHub:** https://github.com/DvidMakesThings/HW_10-In-Rack_PDU

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [System Architecture](#2-system-architecture)
3. [Hardware Configuration](#3-hardware-configuration)
4. [Core System](#4-core-system)
5. [Driver Modules](#5-driver-modules)
6. [Task Modules](#6-task-modules)
7. [Storage Subsystem](#7-storage-subsystem)
8. [Network Services](#8-network-services)
9. [Safety and Protection](#9-safety-and-protection)
10. [Configuration and Calibration](#10-configuration-and-calibration)

---

## 1. Project Overview

The ENERGIS PDU is an intelligent, network-managed Power Distribution Unit designed for 10-inch rack installations. It provides 8 individually controllable relay channels with comprehensive power monitoring, overcurrent protection, and remote management capabilities.

### Key Features

- **8 Relay Channels**: Individual ON/OFF control with hardware verification
- **Real-Time Power Monitoring**: Voltage, current, power, and energy tracking per channel
- **Network Management**: HTTP web interface and SNMP protocol support
- **Overcurrent Protection**: Three-stage protection (WARNING → CRITICAL → LOCKOUT)
- **Persistent Configuration**: Non-volatile storage for all settings and calibration data
- **Front Panel Interface**: Manual control with 4 buttons and LED indicators
- **Safety Features**: Watchdog monitoring, crash logging, and automatic recovery

### Technical Specifications

- **Processor**: Raspberry Pi RP2040 dual-core ARM Cortex-M0+ @ 200 MHz
- **Memory**: 264KB SRAM + 32KB EEPROM (CAT24C256)
- **Network**: W5500 Ethernet controller (10/100 Mbps)
- **Power Measurement**: 8× HLW8032 ICs with per-channel calibration
- **GPIO Expansion**: 3× MCP23017 I2C GPIO expanders (relay, display, selection)
- **Current Capacity**: 10A (EU) or 15A (US) total, configurable per region

---

## 2. System Architecture

### 2.1 Operating System

The firmware runs on **FreeRTOS** with a preemptive multitasking scheduler. The system operates at **200 MHz** for optimal network performance and uses cooperative scheduling between tasks.

**Key Design Principles:**
- Single-owner model for hardware resources (no concurrent access)
- Mutex-protected shared resources
- Queue-based inter-task communication
- Deterministic boot sequencing with explicit dependencies

### 2.2 Task Structure

The system consists of 11 primary tasks:

| Task | Priority | Function | Stack Size |
|------|----------|----------|------------|
| **InitTask** | Highest | Hardware initialization and task creation | Dynamic |
| **HealthTask** | High | Watchdog management and system monitoring | 512 words |
| **StorageTask** | Normal | EEPROM access and configuration management | 2048 words |
| **MeterTask** | Normal | Power measurement and telemetry | 512 words |
| **NetTask** | Normal | Ethernet, HTTP, and SNMP services | 1024 words |
| **SwitchTask** | Normal | Relay and LED control | 512 words |
| **ButtonTask** | Normal | Front panel button handling | 384 words |
| **ConsoleTask** | Normal | UART console and provisioning | 2048 words |
| **LoggerTask** | Low | Centralized logging to USB-CDC | 1024 words |
| **OCP Task** | Normal | Overcurrent protection state machine | 384 words |

### 2.3 Communication Pathways

```
┌─────────────────┐     ┌──────────────┐     ┌─────────────┐
│  Button Task    │────>│ Switch Task  │────>│  MCP23017   │
└─────────────────┘     └──────────────┘     └─────────────┘
                              │
                              v
┌─────────────────┐     ┌──────────────┐     ┌─────────────┐
│  HTTP/SNMP      │────>│ Storage Task │────>│  CAT24C256  │
└─────────────────┘     └──────────────┘     └─────────────┘
                              ^
┌─────────────────┐          │
│  Meter Task     │──────────┘
└─────────────────┘
       │
       v
┌─────────────────┐
│  HLW8032 (x8)   │
└─────────────────┘
```

---

## 3. Hardware Configuration

### 3.1 Microcontroller (RP2040)

**System Clock:** 200 MHz (overclocked from 125 MHz default)
- **Reason:** Improved network performance and SNMP stress handling
- **Note:** FreeRTOSConfig.h must have `configCPU_CLOCK_HZ = 200000000UL`

**Voltage Regulator:** Configured for optimal stability at 200 MHz

### 3.2 GPIO Pin Assignments

| GPIO | Function | Description |
|------|----------|-------------|
| 0-1 | UART0 | HLW8032 power meter communication (RX/TX) |
| 2-3 | I2C1 | EEPROM and relay MCP23017 (SDA/SCL) |
| 4-5 | I2C0 | Display and selection MCP23017 (SDA/SCL) |
| 12-15 | Buttons | Front panel controls (PLUS, MINUS, SET, PWR) |
| 16-21 | SPI0 + GPIO | W5500 Ethernet (MISO, CS, SCK, MOSI, RESET, INT) |
| 23-24 | Reset | MCP23017 hardware reset lines |
| 26, 29 | ADC | VUSB and 12V supply voltage monitoring |
| 28 | GPIO | Processor status LED |

### 3.3 I2C Device Addresses

**I2C1 (400 kHz):**
- `0x50`: CAT24C256 EEPROM
- `0x20`: MCP23017 (Relay board)

**I2C0 (400 kHz):**
- `0x21`: MCP23017 (Display board)
- `0x23`: MCP23017 (Selection LEDs)

### 3.4 SPI Configuration

**SPI0 (40 MHz):**
- W5500 Ethernet controller
- Full-duplex, mode 0, MSB-first

### 3.5 Power Measurement

**8× HLW8032 ICs:**
- Shared UART0 RX line (4800 baud)
- Individual TX lines via 74HC4051 8:1 multiplexer
- Multiplexer control: MCP23017 relay board Port B (MUX_A, MUX_B, MUX_C, MUX_EN)
- Frame format: 24 bytes starting with `0x55 0x5A`

---

## 4. Core System

### 4.1 Main Entry Point

**File:** `energis_rtos.c`

The main entry point performs minimal initialization:
1. Capture boot snapshot for crash detection
2. Configure system clock to 200 MHz
3. Configure peripheral clock
4. Create InitTask (highest priority)
5. Start FreeRTOS scheduler

```c
int main(void) {
    Helpers_EarlyBootSnapshot();
    set_sys_clock_khz(200000, true);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    200000000, 200000000);
    sleep_ms(1000);
    InitTask_Create();
    log_printf("[Main] Starting FreeRTOS scheduler (CPU @ 200 MHz)...\r\n\r\n");
    vTaskStartScheduler();
    // Never returns
}
```

### 4.2 Configuration System

**File:** `config.h`

Central configuration header that defines:

**User-Configurable Settings:**
- Button press durations and debounce times
- Network defaults (IP, subnet, gateway, DNS)
- Overcurrent protection thresholds
- Feature enable/disable flags
- Logging levels (DEBUG, INFO, WARNING, ERROR)

**Hardware Assignments:**
- Peripheral mappings (I2C, SPI, UART)
- GPIO pin definitions
- ADC channel assignments
- Device addresses

**Logging Macros:**
```c
DEBUG_PRINT(...)        // Detailed debug information
INFO_PRINT(...)         // Informational messages
WARNING_PRINT(...)      // Warning conditions
ERROR_PRINT(...)        // Error conditions
ERROR_PRINT_CODE(...)   // Errors with 16-bit diagnostic codes
```

### 4.3 Error Code System

**File:** `error_code.h`

Hierarchical 16-bit error code scheme: `0xMSCC`

**Format:**
- **M (bits 15-12):** Module ID (INIT, NET, METER, STORAGE, BUTTON, HEALTH, etc.)
- **S (bits 11-8):** Severity (INFO=0x1, WARNING=0x2, ERROR=0x4, FATAL=0xF)
- **C (bits 7-4):** File ID within module
- **C (bits 3-0):** Error ID within file

**Example:**
```c
// Network task HTTP handler error
uint16_t errorcode = ERR_MAKE_CODE(ERR_MOD_NET, ERR_SEV_ERROR,
                                   ERR_FID_NET_HTTP_SERVER, 0x5);
ERROR_PRINT_CODE(errorcode, "HTTP socket bind failed\r\n");
Storage_EnqueueErrorCode(errorcode);  // Log to EEPROM
```

**Key Macros:**
```c
ERR_MAKE_CODE(module, severity, fid, eid)  // Construct code
ERR_GET_MODULE(code)                        // Extract module
ERR_GET_SEVERITY(code)                      // Extract severity
ERR_GET_FID(code)                           // Extract file ID
ERR_GET_EID(code)                           // Extract error ID
```

---

## 5. Driver Modules

### 5.1 Button Driver

**Files:** `drivers/button_driver.h`, `drivers/button_driver.c`

Provides hardware abstraction for front-panel controls:
- **3 Pushbuttons:** PLUS, MINUS, SET (active-low with pull-ups)
- **8 Selection LEDs:** Driven via MCP23017 GPIO expander

**Key Functions:**

```c
// Initialization
void ButtonDrv_InitGPIO(void);

// Button state reads (no debouncing)
bool ButtonDrv_ReadPlus(void);
bool ButtonDrv_ReadMinus(void);
bool ButtonDrv_ReadSet(void);

// Selection LED control
void ButtonDrv_SelectAllOff(void);
void ButtonDrv_SelectShow(uint8_t index, bool on);
void ButtonDrv_SelectLeft(uint8_t *io_index, bool led_on);   // Decrement with wraparound
void ButtonDrv_SelectRight(uint8_t *io_index, bool led_on);  // Increment with wraparound

// Button actions
void ButtonDrv_DoSetShort(uint8_t index);  // Toggle relay
void ButtonDrv_DoSetLong(void);            // Clear fault LED

// Timing utility
uint32_t ButtonDrv_NowMs(void);  // Milliseconds since boot
```

**Design Notes:**
- No debouncing performed at driver level (handled by ButtonTask)
- LED control delegated to SwitchTask for thread-safe I2C operations
- Direct GPIO reads for minimal latency

### 5.2 Ethernet Driver (W5500)

**Files:** `drivers/ethernet_driver.h`, `drivers/ethernet_driver.c`, `drivers/ethernet_config.h`, `drivers/ethernet_w5500regs.h`

Thread-safe driver for Wiznet W5500 hardwired TCP/IP Ethernet controller.

**Features:**
- Mutex-protected SPI access for concurrent task safety
- 8 hardware sockets for TCP/UDP protocols
- DMA-compatible buffer operations
- PHY auto-negotiation (10/100 Mbps, half/full duplex)
- Configurable per-socket RX/TX buffer sizes (max 16KB each)

**Architecture:**
```
Application Layer
     ↓
Socket API (socket.h)
     ↓
W5500 Register Access (ethernet_driver.h)
     ↓
Mutex-Protected SPI (w5500_spi_mutex)
     ↓
RP2040 SPI0 @ 40 MHz
```

**Initialization:**

```c
// 1. Hardware init (SPI, GPIO, reset sequence)
bool w5500_hw_init(void);

// 2. Chip init (PHY, network config, buffer allocation)
w5500_NetConfig netcfg = {
    .mac = {0x02, 0x45, 0x4E, 0x00, 0x00, 0x01},
    .ip = {192, 168, 0, 22},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 0, 1},
    .dns = {8, 8, 8, 8},
    .dhcp = EEPROM_NETINFO_STATIC
};
bool w5500_chip_init(&netcfg);

// 3. Version check (diagnostic)
bool w5500_check_version(void);  // Should return true (0x04)
```

**Register Access:**

All register operations are mutex-protected and use 32-bit address selectors:

```c
// Low-level register access
uint8_t w5500_read_reg(uint32_t addr_sel);
void w5500_write_reg(uint32_t addr_sel, uint8_t data);
void w5500_read_buf(uint32_t addr_sel, uint8_t *buf, uint16_t len);
void w5500_write_buf(uint32_t addr_sel, uint8_t *buf, uint16_t len);

// Common register helpers (inline macros)
setMR(mode);                     // Mode register
setSIPR(ip_array);               // Source IP
setSHAR(mac_array);              // Source MAC
setGAR(gw_array);                // Gateway
setSUBR(subnet_array);           // Subnet mask
```

**Socket Operations:**

```c
// Socket configuration
setSn_MR(socket_num, mode);      // Socket mode (TCP/UDP/MACRAW)
setSn_PORT(socket_num, port);    // Local port (host byte order)
setSn_DPORT(socket_num, port);   // Destination port
setSn_DIPR(socket_num, ip);      // Destination IP

// Socket commands
setSn_CR(socket_num, Sn_CR_OPEN);     // Open socket
setSn_CR(socket_num, Sn_CR_LISTEN);   // TCP listen
setSn_CR(socket_num, Sn_CR_CONNECT);  // TCP connect
setSn_CR(socket_num, Sn_CR_SEND);     // Send data
setSn_CR(socket_num, Sn_CR_RECV);     // Receive data
setSn_CR(socket_num, Sn_CR_CLOSE);    // Close socket

// Status queries
uint8_t status = getSn_SR(socket_num);
uint16_t rx_avail = getSn_RX_RSR(socket_num);
uint16_t tx_free = getSn_TX_FSR(socket_num);
```

**Data Transfer:**

```c
// Send data to TX buffer
void eth_send_data(uint8_t sn, uint8_t *data, uint16_t len);

// Receive data from RX buffer
void eth_recv_data(uint8_t sn, uint8_t *data, uint16_t len);

// Discard received data
void eth_recv_ignore(uint8_t sn, uint16_t len);
```

**PHY Management:**

```c
// Get link status
w5500_PhyLink link = w5500_get_link_status();  // PHY_LINK_ON or PHY_LINK_OFF

// Configure PHY mode
w5500_PhyConfig phycfg = {
    .by = PHY_CONFBY_SW,         // Software configuration
    .mode = PHY_MODE_AUTONEGO,   // Auto-negotiation
    .speed = PHY_SPEED_100,      // 100 Mbps
    .duplex = PHY_DUPLEX_FULL    // Full duplex
};
w5500_set_phy_conf(&phycfg);

// Power management
w5500_set_phy_power(PHY_POWER_NORM);  // or PHY_POWER_DOWN
```

**Network Configuration:**

```c
// Apply network settings
void w5500_set_network(w5500_NetConfig *netcfg);

// Read current settings
w5500_NetConfig current;
w5500_get_network(&current);

// Print configuration (diagnostic)
w5500_print_network(&current);
```

### 5.3 HLW8032 Power Measurement Driver

**Files:** `drivers/hlw8032_driver.h`, `drivers/hlw8032_driver.c`

RTOS-safe driver for 8× HLW8032 power measurement ICs with shared UART and multiplexed channel selection.

**Hardware Architecture:**
```
RP2040 UART0 TX ──> 74HC4051 MUX ──> HLW8032 CH0-7 TX
RP2040 UART0 RX <── HLW8032 CH0-7 RX (wired-OR)
MCP23017 Port B ──> MUX Control (A,B,C,EN)
```

**Key Features:**
- Mutex-protected UART access (`uartHlwMtx`)
- Hardware v1.0.0 pin swap compensation
- Per-channel calibration with EEPROM persistence
- Cached measurements for non-blocking reads
- Total current sum for overcurrent protection
- Channel uptime tracking (ON-time accumulation)

**Initialization:**

```c
void hlw8032_init(void);  // Called by MeterTask during boot
```

Creates UART mutex, initializes UART0 @ 4800 baud, loads calibration from EEPROM.

**Measurement Functions:**

```c
// Blocking read (acquires mutex, selects channel, waits for frame)
bool hlw8032_read(uint8_t channel);  // Returns true if valid frame received

// Get last reading (from previous hlw8032_read call)
float voltage = hlw8032_get_voltage();
float current = hlw8032_get_current();
float power = hlw8032_get_power();

// Round-robin polling (called by MeterTask in loop)
void hlw8032_poll_once(void);  // Polls one channel per call, cycles through 0-7

// Refresh all channels (blocking, ~2 seconds)
void hlw8032_refresh_all(void);  // Sequential polling of all 8 channels
```

**Cached Data Access (Non-Blocking):**

```c
// Get cached measurements (updated by poll_once/refresh_all)
float v = hlw8032_cached_voltage(uint8_t ch);
float i = hlw8032_cached_current(uint8_t ch);
float p = hlw8032_cached_power(uint8_t ch);
uint32_t uptime = hlw8032_cached_uptime(uint8_t ch);
bool state = hlw8032_cached_state(uint8_t ch);

// Total current across all channels (for overcurrent protection)
float total_current = hlw8032_get_total_current();

// Check if measurement cycle completed
bool cycle_done = hlw8032_cycle_complete();  // Flag cleared after read
```

**Calibration System:**

Each channel has independent calibration parameters stored in EEPROM:

```c
typedef struct {
    uint8_t calibrated;       // 0xCA if calibrated, 0xFF if not
    float voltage_factor;     // Voltage calibration multiplier
    float voltage_offset;     // Voltage offset in volts
    float current_factor;     // Current calibration multiplier
    float current_offset;     // Current offset in amps
    float resistor_value;     // Shunt resistor value (ohms)
    uint8_t reserved[23];     // Reserved for future use
} hlw_calib_t;
```

**Calibration API:**

```c
// Load calibration from EEPROM (auto-called by init)
void hlw8032_load_calibration(void);

// Get calibration for a channel
bool hlw8032_get_calibration(uint8_t channel, hlw_calib_t *calib);

// Print calibration to log
void hlw8032_print_calibration(uint8_t channel);

// Asynchronous calibration sequences (driven by poll_once)
bool hlw8032_calibration_start_zero_all(void);
bool hlw8032_calibration_start_voltage_all(float ref_voltage);
bool hlw8032_calibration_start_current_single(uint8_t channel, float ref_current);
bool hlw8032_calibration_is_running(void);
```

**Uptime Tracking:**

```c
// Update uptime based on relay state (called by poll_once)
void hlw8032_update_uptime(uint8_t ch, bool relay_state);

// Get accumulated uptime
uint32_t uptime_seconds = hlw8032_get_uptime(uint8_t ch);
```

**Diagnostic Functions:**

```c
// Dump all cached values to log
void hlw8032_dump_cache(void);
```

**Multiplexer Control:**

Channel selection is handled automatically by `hlw8032_read()`:

```c
// Internal: Select channel via MCP23017 Port B
MUX_A = (channel & 0x01) ? 1 : 0;
MUX_B = (channel & 0x02) ? 1 : 0;
MUX_C = (channel & 0x04) ? 1 : 0;
MUX_EN = 0;  // Active-low enable
// Hardware settling delay: 1ms
```

### 5.4 MCP23017 GPIO Expander Driver

**Files:** `drivers/mcp23017_driver.h`, `drivers/mcp23017_driver.c`

Simplified, thread-safe driver for up to 8× MCP23017 I2C GPIO expanders with per-device mutex protection.

**Design Principles:**
- Direct I2C operations with minimal abstraction
- Per-device mutex for thread safety
- Shadow OLAT registers to prevent torn read-modify-write
- Timeout-based I2C with automatic retries
- Clear error reporting

**Device Context:**

```c
typedef struct {
    i2c_inst_t *i2c;         // I2C bus (i2c0 or i2c1)
    uint8_t addr;            // 7-bit address (0x20-0x27)
    int8_t rst_gpio;         // Reset GPIO (-1 if unused)
    volatile uint8_t olat_a; // Shadow of OLATA register
    volatile uint8_t olat_b; // Shadow of OLATB register
    SemaphoreHandle_t mutex; // Per-device mutex
    bool inited;             // Initialization flag
} mcp23017_t;
```

**Board-Specific Initialization:**

```c
// Initialize all three MCP23017 devices for ENERGIS PDU
void MCP2017_Init(void);

// Get device handles
mcp23017_t *mcp_relay(void);      // Relay board (I2C1 @ 0x20)
mcp23017_t *mcp_display(void);    // Display board (I2C0 @ 0x21)
mcp23017_t *mcp_selection(void);  // Selection LEDs (I2C0 @ 0x23)
```

**Device Registration (for custom applications):**

```c
// Register or retrieve device context
mcp23017_t *dev = mcp_register(i2c1, 0x20, MCP_MB_RST);

// Initialize device to known state
mcp_init(dev);  // Sets BANK=0, all outputs, no pull-ups
```

**Register Operations:**

```c
// Write register (thread-safe, updates shadows)
bool ok = mcp_write_reg(dev, MCP23017_IODIRA, 0x00);  // All outputs

// Read register (thread-safe)
uint8_t value;
bool ok = mcp_read_reg(dev, MCP23017_GPIOA, &value);
```

**Pin Operations:**

```c
// Set pin direction (0=output, 1=input)
mcp_set_direction(dev, pin, 0);  // Pin 0-15 (0-7=Port A, 8-15=Port B)

// Write pin state (0=low, 1=high)
bool ok = mcp_write_pin(dev, pin, 1);

// Read pin state
uint8_t state = mcp_read_pin(dev, pin);  // Returns 0 or 1
```

**Atomic Multi-Bit Operations:**

```c
// Masked write (preserves unmasked bits)
bool ok = mcp_write_mask(dev, port_ab, mask, value_bits);

// Example: Set pins 0 and 2 without affecting other pins
mcp_write_mask(dev, 0, 0b00000101, 0b00000101);  // Port A, pins 0 and 2 high
```

**Shadow Register Management:**

```c
// Re-sync shadows from hardware (use after external changes)
mcp_resync_from_hw(dev);
```

**Error Recovery:**

```c
// Soft recovery (reprogram registers without hardware reset)
bool ok = mcp_recover(dev);
```

**Configuration Constants:**

```c
#define MCP_I2C_MAX_RETRIES 3
#define MCP_I2C_TIMEOUT_US 5000
#define MCP_I2C_RETRY_DELAY_US 200
#define MCP_RESET_PULSE_MS 5
#define MCP_POST_RESET_MS 10
```

### 5.5 CAT24C256 EEPROM Driver

**Files:** `drivers/cat24c256_driver.h`, `drivers/cat24c256_driver.c`

I2C driver for 32KB serial EEPROM with page-aware write operations and write cycle management.

**Key Features:**
- 64-byte page boundary handling
- Mandatory write cycle delays (5ms per page)
- 16-bit address space (0x0000 - 0x7FFF)
- Thread-safe via StorageTask single-owner model
- Self-test function for hardware validation

**Initialization:**

```c
void CAT24C256_Init(void);  // Called by InitTask
```

Configures I2C GPIO pins with pull-ups. I2C peripheral must be initialized first.

**Basic Operations:**

```c
// Write single byte (blocking, includes 5ms delay)
int result = CAT24C256_WriteByte(uint16_t addr, uint8_t data);

// Read single byte
uint8_t data = CAT24C256_ReadByte(uint16_t addr);
```

**Buffer Operations:**

```c
// Write buffer with automatic page boundary handling
int result = CAT24C256_WriteBuffer(uint16_t addr, const uint8_t *data, uint16_t len);
```

**Page Boundary Example:**
```
Write 100 bytes starting at 0x0020:
  - Chunk 1: 0x0020-0x003F (32 bytes to boundary) → 5ms delay
  - Chunk 2: 0x0040-0x007F (64 bytes, full page) → 5ms delay
  - Chunk 3: 0x0080-0x0083 (4 bytes remaining) → 5ms delay
Total operation time: 15ms + I2C transfer time
```

```c
// Read buffer (no page restrictions)
void CAT24C256_ReadBuffer(uint16_t addr, uint8_t *buffer, uint32_t len);
```

**Self-Test:**

```c
// Test EEPROM with 8-byte pattern
bool ok = CAT24C256_SelfTest(uint16_t test_addr);
```

Test pattern designed to detect common failures:
- `0xAA, 0x55`: Alternating bits (stuck bit detection)
- `0xCC, 0x33`: Adjacent bit pairs (crosstalk detection)
- `0xF0, 0x0F`: Nibble patterns (partial byte errors)
- `0x00, 0xFF`: Extreme values (threshold issues)

**Thread Safety Note:**

All EEPROM operations must be called from StorageTask with `eepromMtx` held. Other tasks should use StorageTask queue API instead of direct driver access.

---

## 6. Task Modules

### 6.1 InitTask - System Initialization

**Files:** `tasks/inittask.h`, `tasks/inittask.c`

Highest-priority task responsible for hardware bring-up and task creation in proper sequence.

**Responsibilities:**
1. Initialize all hardware in dependency order
2. Probe peripherals to verify communication
3. Create subsystem tasks with deterministic sequencing
4. Wait for subsystems to report ready
5. Apply saved configuration (relay states) on startup
6. Delete itself when system is fully operational

**Boot Sequence:**

```
main()
  └─> InitTask_Create()
       └─> InitTask runs at highest priority
            1. Initialize LoggerTask
            2. Initialize HealthTask (watchdog)
            3. Initialize I2C buses
            4. Initialize MCP23017 GPIO expanders
            5. Initialize EEPROM driver
            6. Initialize ButtonTask
            7. Initialize StorageTask (loads config)
            8. Initialize SwitchTask (relay control)
            9. Initialize MeterTask (power monitoring)
           10. Initialize NetTask (HTTP + SNMP)
           11. Apply startup relay configuration
           12. Signal system ready
           13. Delete InitTask
```

**API:**

```c
// Create InitTask (called from main)
void InitTask_Create(void);

// Save current relay states as startup config
bool InitTask_SaveCurrentRelayStates(void);
```

**Deterministic Sequencing:**

Each task waits for its dependencies:
- **StorageTask** waits for ConsoleTask
- **SwitchTask** waits for StorageTask
- **MeterTask** waits for NetTask
- **NetTask** waits for SwitchTask

Timeout guards prevent indefinite blocking (5 seconds per dependency).

### 6.2 HealthTask - Watchdog and System Monitoring

**Files:** `tasks/healthtask.h`, `tasks/healthtask.c`

Manages RP2040 hardware watchdog and monitors system health metrics.

**Key Features:**
- Periodic watchdog feeding (configurable interval)
- Task heartbeat monitoring (detects hung tasks)
- System uptime tracking
- Reboot reason detection (power-on, watchdog, crash)
- Crash log integration
- Cooperative scheduling health checks

**Watchdog Configuration:**

```c
#define HEALTH_WATCHDOG_TIMEOUT_MS 2000  // 2-second watchdog
#define HEALTH_HEARTBEAT_INTERVAL_MS 500 // Feed every 500ms
```

**Reboot Reasons:**

```c
typedef enum {
    REBOOT_REASON_POWER_ON,      // Clean power-on reset
    REBOOT_REASON_WATCHDOG,      // Watchdog timeout
    REBOOT_REASON_CRASH,         // Software crash detected
    REBOOT_REASON_DEBUGGER,      // Debugger reset
    REBOOT_REASON_UNKNOWN        // Unable to determine
} reboot_reason_t;
```

**API:**

```c
// Initialize HealthTask
BaseType_t HealthTask_Init(bool enable);

// Check readiness
bool Health_IsReady(void);

// Get system uptime
uint32_t Health_GetUptimeSeconds(void);

// Get reboot reason
reboot_reason_t Health_GetRebootReason(void);

// Register task heartbeat (called by each task periodically)
void Health_TaskHeartbeat(TaskHandle_t task_handle);
```

**Heartbeat Protocol:**

Each task periodically calls `Health_TaskHeartbeat()`:
```c
void task_function(void *params) {
    while (1) {
        // Do work
        Health_TaskHeartbeat(xTaskGetCurrentTaskHandle());
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

If a task stops sending heartbeats, HealthTask logs a warning and may trigger a watchdog reset.

### 6.3 StorageTask - Configuration Management

**Files:** `tasks/storagetask.h`, `tasks/storagetask.c`, `tasks/storage_submodule/*`

Single-owner manager for all persistent configuration in CAT24C256 EEPROM.

**Architecture:**
- **Queue-based API:** All external access via `q_cfg` message queue
- **RAM caching:** Critical config mirrored for fast access
- **Write debouncing:** 2-second idle period before EEPROM commit
- **Modular design:** Dedicated submodules per EEPROM section

**Configuration Sections:**

| Section | Submodule | Description |
|---------|-----------|-------------|
| Factory Defaults | factory_defaults.c | First-boot detection and initialization |
| User Output | user_output.c | Relay states, presets (5 + startup) |
| Network | network.c | IP, subnet, gateway, DNS, MAC, DHCP mode |
| Calibration | calibration.c | HLW8032 per-channel coefficients |
| Energy Monitor | energy_monitor.c | Historical energy consumption logs |
| Event Log | event_log.c | Error and warning ring buffers |
| User Prefs | user_prefs.c | Device name, location, display settings |
| Channel Labels | channel_labels.c | 8×64-char custom names (RAM cached) |
| Device Identity | device_identity.c | Serial number, region, MAC derivation |

**Initialization:**

```c
void StorageTask_Init(bool enable);

// Wait for configuration to be loaded
bool storage_wait_ready(uint32_t timeout_ms);

// Check readiness
bool Storage_IsReady(void);
```

**Network Configuration API:**

```c
// Get network config (from RAM cache, non-blocking)
networkInfo net;
bool ok = storage_get_network(&net);

// Set network config (update cache, schedule EEPROM write)
networkInfo new_net = {
    .ip = {192, 168, 1, 100},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 1, 1},
    .dns = {8, 8, 8, 8},
    .dhcp = EEPROM_NETINFO_STATIC
};
memcpy(new_net.mac, existing_mac, 6);
bool ok = storage_set_network(&new_net);

// Force immediate commit (bypasses debounce)
bool ok = storage_commit_now(5000);  // 5-second timeout
```

**User Preferences API:**

```c
// Get preferences
userPrefInfo prefs;
bool ok = storage_get_prefs(&prefs);

// Set preferences
userPrefInfo new_prefs = {
    .device_name = "ENERGIS-PDU-001",
    .location = "Server Rack A",
    .display_brightness = 75
};
bool ok = storage_set_prefs(&new_prefs);
```

**Relay State Persistence:**

```c
// Get startup relay states
uint8_t states[8];  // 0=OFF, 1=ON
bool ok = storage_get_relay_states(states);

// Set startup relay states
uint8_t new_states[8] = {1, 1, 0, 0, 1, 1, 0, 0};
bool ok = storage_set_relay_states(new_states);
```

**Channel Labels API:**

```c
// Get single channel label (from RAM cache)
char label[65];
bool ok = storage_get_channel_label(uint8_t channel, label, sizeof(label));

// Set single channel label (cache + EEPROM)
bool ok = storage_set_channel_label(uint8_t channel, "Web Server");

// Get all labels at once
char all_labels[8][65];
bool ok = storage_get_all_channel_labels(all_labels, 65);
```

**Sensor Calibration API:**

```c
// Get calibration for channel
hlw_calib_t calib;
bool ok = storage_get_sensor_cal(uint8_t channel, &calib);

// Set calibration for channel
hlw_calib_t new_calib = {
    .calibrated = 0xCA,
    .voltage_factor = 1.05,
    .voltage_offset = -0.5,
    .current_factor = 1.02,
    .current_offset = 0.0,
    .resistor_value = 0.001
};
bool ok = storage_set_sensor_cal(uint8_t channel, &new_calib);
```

**Event Logging API:**

```c
// Enqueue error code (non-blocking, safe from any context)
Storage_EnqueueErrorCode(uint16_t error_code);

// Enqueue warning code
Storage_EnqueueWarningCode(uint16_t warning_code);

// Dump error log region (async)
bool ok = storage_dump_error_log_async();

// Dump warning log region (async)
bool ok = storage_dump_warning_log_async();

// Clear error log region (async)
bool ok = storage_clear_error_log_async();

// Clear warning log region (async)
bool ok = storage_clear_warning_log_async();
```

**Preset Management API:**

```c
// Save relay preset (index 0-4)
bool ok = storage_save_preset(uint8_t index, "Office Hours", 0b11111100);

// Delete preset
bool ok = storage_delete_preset(uint8_t index);

// Set startup preset (applied at boot)
bool ok = storage_set_startup_preset(uint8_t index);

// Clear startup preset (all OFF at boot)
bool ok = storage_clear_startup_preset();
```

**Factory Reset API:**

```c
// Reset all config to factory defaults
bool ok = storage_load_defaults(5000);  // 5-second timeout
```

**Maintenance API:**

```c
// Full EEPROM erase (async, watchdog-safe)
bool ok = storage_erase_all_async();

// Check if erase in progress
bool busy = storage_erase_all_is_busy();

// Dump entire EEPROM in hex format (async)
bool ok = storage_dump_formatted_async();
```

**Write Debouncing:**

Configuration changes mark sections as "dirty" and reset a 2-second timer. When the timer expires with no new changes, pending writes are committed to EEPROM automatically. This extends EEPROM lifetime during configuration workflows.

Manual commit available via `storage_commit_now()` for immediate persistence.

### 6.4 MeterTask - Power Monitoring

**Files:** `tasks/metertask.h`, `tasks/metertask.c`

Exclusive owner of HLW8032 power measurement hardware and RP2040 ADC subsystem.

**Key Features:**
- Round-robin HLW8032 polling at 25 Hz (40ms period)
- System ADC telemetry at 5 Hz (200ms period)
- Energy accumulation in kWh with millisecond resolution
- Relay uptime tracking per channel
- Temperature calibration support (1-point and 2-point)
- Overcurrent protection integration
- Standby mode support (pauses measurements)

**Telemetry Structures:**

```c
// Per-channel power measurement
typedef struct {
    uint8_t channel;       // Channel 0-7
    float voltage;         // RMS voltage [V]
    float current;         // RMS current [A]
    float power;           // Active power [W]
    float power_factor;    // Power factor [0..1]
    uint32_t uptime;       // Accumulated ON-time [s]
    float energy_kwh;      // Accumulated energy [kWh]
    bool relay_state;      // Relay ON/OFF
    uint32_t timestamp_ms; // Sample timestamp
    bool valid;            // Measurement validity
} meter_telemetry_t;

// System-level ADC telemetry
typedef struct {
    float die_temp_c;      // RP2040 temperature [°C]
    float vusb_volts;      // USB voltage [V]
    float vsupply_volts;   // 12V supply voltage [V]
    uint16_t raw_temp;     // Raw ADC codes (diagnostic)
    uint16_t raw_vusb;
    uint16_t raw_vsupply;
    uint32_t timestamp_ms;
    bool valid;
} system_telemetry_t;
```

**Initialization:**

```c
BaseType_t MeterTask_Init(bool enable);
bool Meter_IsReady(void);
```

**Telemetry Access:**

```c
// Get latest cached telemetry (non-blocking)
meter_telemetry_t telem;
bool ok = MeterTask_GetTelemetry(uint8_t channel, &telem);

// Force immediate refresh (blocking, ~40ms)
MeterTask_RefreshAll();

// Get system ADC telemetry (non-blocking)
system_telemetry_t sys;
bool ok = MeterTask_GetSystemTelemetry(&sys);
```

**Temperature Calibration:**

Die temperature formula:
```
T[°C] = 27 - (V - V0) / S + OFFSET
where V = raw_adc × (VREF / ADC_MAX)
```

```c
// Compute single-point calibration (offset only)
float v0, slope, offset;
bool ok = MeterTask_TempCalibration_SinglePointCompute(
    float ambient_c, uint16_t raw_temp, &v0, &slope, &offset);

// Compute two-point calibration (slope + intercept)
bool ok = MeterTask_TempCalibration_TwoPointCompute(
    float t1_c, uint16_t raw1, float t2_c, uint16_t raw2,
    &v0, &slope, &offset);

// Apply calibration parameters
bool ok = MeterTask_SetTempCalibration(v0, slope, offset);

// Query active calibration
uint8_t mode;  // 0=NONE, 1=1PT, 2=2PT
bool ok = MeterTask_GetTempCalibrationInfo(&mode, &v0, &slope, &offset);
```

**Telemetry Queue:**

Consumer tasks can read from the telemetry queue:
```c
extern QueueHandle_t q_meter_telemetry;

meter_telemetry_t sample;
if (xQueueReceive(q_meter_telemetry, &sample, 0) == pdTRUE) {
    // Process sample
}
```

**Measurement Flow:**

1. HLW8032 polling: 40ms period, 8 channels × ~5ms each
2. Instantaneous values cached per-channel
3. Energy integration: power × time accumulated in kWh
4. Power factor calculation: P / (V × I) clamped to [0, 1]
5. Telemetry publication: every 5th sample (~200ms) to queue
6. ADC sampling: 200ms period for system telemetry

**Standby Mode:**

When system enters standby:
- Stops all HLW8032 polling
- Stops ADC sampling
- Maintains heartbeat at 500ms for watchdog

On exit from standby:
- Detects state transition
- Resumes normal operation

### 6.5 SwitchTask - Relay and LED Control

**Files:** `tasks/switchtask.h`, `tasks/switchtask.c`

Synchronous relay and LED control manager with deterministic operations and comprehensive error handling.

**Design Philosophy:**
- **Synchronous operations:** All functions block until complete
- **Immediate feedback:** Success/failure known before return
- **No hidden state:** Every operation directly reflects hardware intent
- **Mutex-protected:** Thread-safe from multiple tasks
- **Detailed result codes:** Enable proper error handling

**Architecture:**
- Single global mutex serializes all MCP23017 I2C access
- Direct driver calls (no internal queues)
- Hardware state caching reduces I2C traffic
- Relay-to-LED mirroring for visual feedback
- Periodic hardware sync (5-second interval)

**Hardware Configuration:**

| Device | Address | Port A | Port B |
|--------|---------|--------|--------|
| MCP_RELAY | 0x20 | Relays 0-7 | MUX control (unused) |
| MCP_DISPLAY | 0x21 | Output LEDs 1-4 | Selection LEDs 0-3 |
| MCP_LED | 0x22 | Output LEDs 5-8 | Selection LEDs 4-7, PWR LED, ETH LED, FAULT LED |

**Result Codes:**

```c
typedef enum {
    SWITCH_OK = 0,              // Operation successful
    SWITCH_ERR_NOT_INIT,        // Subsystem not initialized
    SWITCH_ERR_INVALID_CHANNEL, // Channel out of range (0-7)
    SWITCH_ERR_MUTEX_TIMEOUT,   // Could not acquire mutex
    SWITCH_ERR_I2C_FAIL,        // I2C communication failed
    SWITCH_ERR_VERIFY_FAIL,     // Read-back verification failed
    SWITCH_ERR_OVERCURRENT,     // Rejected due to OCP lockout
    SWITCH_ERR_NULL_PARAM       // NULL parameter provided
} switch_result_t;
```

**Initialization:**

```c
BaseType_t SwitchTask_Init(bool enable);
bool Switch_IsReady(void);
```

**Relay Control:**

```c
// Set single channel (synchronous, blocks until verified)
switch_result_t result = Switch_SetChannel(uint8_t channel, bool state);

// Toggle single channel
switch_result_t result = Switch_Toggle(uint8_t channel);

// Set all channels ON
switch_result_t result = Switch_AllOn();

// Set all channels OFF
switch_result_t result = Switch_AllOff();

// Set multiple channels via bitmask
switch_result_t result = Switch_SetMask(uint8_t mask);  // Bit N = channel N
```

**Relay State Queries:**

```c
// Get single channel state from hardware
bool state;
switch_result_t result = Switch_GetState(uint8_t channel, &state);

// Get all channel states as bitmask
uint8_t mask;
switch_result_t result = Switch_GetAllStates(&mask);

// Get cached mask (non-blocking, may be stale)
uint8_t mask = Switch_GetCachedMask();
```

**Selection LED Control:**

```c
// Turn off all selection LEDs
bool ok = Switch_SelectAllOff(0);

// Show selection LED
bool ok = Switch_SelectShow(uint8_t index, bool on, 0);
```

**Status LED Control:**

```c
// Fault LED
bool ok = Switch_SetFaultLed(bool state, 0);

// Power Good LED
bool ok = Switch_SetPwrLed(bool state, 0);

// Network Link LED
bool ok = Switch_SetEthLed(bool state, 0);
```

**MUX Control (for HLW8032):**

```c
// Set relay MCP Port B masked bits
bool ok = Switch_SetRelayPortBMasked(uint8_t mask, uint8_t value, uint32_t timeout_ms);
```

This function is designed to **never fail** from caller's perspective:
- If mutex available: executes immediately
- If mutex busy: stores pending request for later

**Hardware Synchronization:**

```c
// Force sync of cache from hardware
bool ok = Switch_SyncFromHardware(0);
```

Automatically performed every 5 seconds by background task.

**Manual Panel Activity Gate:**

```c
// Enable/disable front-panel selection updates
Switch_SetManualPanelActive(bool active);
```

When disabled (default), selection MCP (0x23) is not written. ButtonTask enables this during user interaction.

**Relay Control Flow:**

1. Caller invokes `Switch_SetChannel(channel, state)`
2. Mutex acquired with 1-second timeout
3. Overcurrent check: rejects if OCP in lockout
4. Relay write via MCP_RELAY driver
5. LED mirror write via MCP_DISPLAY (best-effort)
6. Hardware read-back verification
7. Cache update if verification successful
8. Mutex released
9. Result code returned

**Overcurrent Protection Integration:**

Before turning ON a relay, SwitchTask queries `Overcurrent_IsSwitchingAllowed()`:
- If OCP in lockout: returns `SWITCH_ERR_OVERCURRENT`
- If OCP allows: proceeds with relay closure
- Relay OFF operations: always allowed (safety)

### 6.6 ButtonTask - Front Panel Interface

**Files:** `tasks/buttontask.h`, `tasks/buttontask.c`

Manages front-panel button interface with debouncing and long-press detection.

**Buttons:**
- **PLUS:** Increment channel selection
- **MINUS:** Decrement channel selection
- **SET:** Toggle selected channel (short press) or clear fault LED (long press)
- **PWR:** (Reserved for future use)

**Key Features:**
- Software debouncing (100ms)
- Long-press detection (2500ms threshold)
- Channel selection with wraparound (0↔7)
- Visual LED feedback
- Post-action guard time (110ms)

**State Machine:**

```
IDLE ──[button press]──> DEBOUNCE_WAIT (100ms)
                              │
                              v
                         ACTIVE ──[button hold >2500ms]──> LONG_PRESS
                              │
                              v
                         [button release] ──> SHORT_PRESS ──> POST_GUARD (110ms) ──> IDLE
```

**Configuration:**

```c
#define DEBOUNCE_MS 100u        // Debounce period
#define LONGPRESS_DT 2500       // Long press threshold
#define POST_GUARD_MS 110u      // Post-action guard
```

**Button Actions:**

**PLUS (short):** Increment selection
```c
current_index = (current_index + 1) % 8;  // Wraparound at 7→0
ButtonDrv_SelectShow(current_index, true);
```

**MINUS (short):** Decrement selection
```c
current_index = (current_index == 0) ? 7 : (current_index - 1);  // Wraparound at 0→7
ButtonDrv_SelectShow(current_index, true);
```

**SET (short):** Toggle relay
```c
ButtonDrv_DoSetShort(current_index);  // Calls Switch_Toggle()
```

**SET (long):** Clear fault LED
```c
ButtonDrv_DoSetLong();  // Calls Switch_SetFaultLed(false, 0)
```

**Manual Panel Activity Window:**

ButtonTask automatically manages the selection MCP (0x23) activity gate:
- Opens gate when user presses a button (enables selection writes)
- Closes gate after 5 seconds of inactivity (disables selection writes)
- Prevents unnecessary I2C traffic when panel not in use

### 6.7 NetTask - Network Services

**Files:** `tasks/nettask.h`, `tasks/nettask.c`

Network stack manager for W5500 Ethernet, HTTP server, and SNMP agent.

**Architecture:**
- Single-owner model: exclusive access to W5500 hardware
- Manages 3 W5500 sockets: HTTP (0), SNMP agent (1), SNMP trap (2)
- Link supervision with automatic reinitialization
- Standby mode support (suspends during power-saving)
- ETH LED visual feedback (solid when linked, blinking when down)

**Key Features:**
- HTTP web server for PDU control interface and metrics endpoint
- SNMP agent for network management protocol support
- SNMP trap sender for proactive event notifications
- Robust link detection with automatic recovery
- Full reinitialization on each link-up event
- Graceful degradation (system operates without Ethernet)

**Initialization:**

```c
BaseType_t NetTask_Init(bool enable);
bool Net_IsReady(void);
```

**Network Configuration:**

```c
// Apply configuration from StorageTask
bool ok = ethernet_apply_network_from_storage(const networkInfo *ni);
```

Converts persistent network configuration into W5500 register settings:
1. Map `networkInfo` to `w5500_NetConfig`
2. Call `w5500_chip_init()` to program registers
3. Verify and log results
4. Return success/failure

**Link Supervision:**

NetTask continuously polls W5500 PHY status:
- **Link-up transition:** Triggers full W5500 reset and service restart
- **Link-down transition:** Enables ETH LED blinking (1 Hz)
- **Prevention:** Avoids ERR_CONNECTION_REFUSED after cable reconnection

**Service Management:**

| Service | Socket | Port | Protocol |
|---------|--------|------|----------|
| HTTP server | 0 | 80 | TCP |
| SNMP agent | 1 | 161 | UDP |
| SNMP trap sender | 2 | Dynamic | UDP |

**Standby Mode:**

When system enters standby:
- W5500 held in hardware reset by power manager
- All network processing suspended
- Maintains heartbeat (500ms) for watchdog

On exit from standby:
- Detects state transition
- Calls `net_reinit_from_cache()`
- Restores W5500 configuration
- Re-evaluates link status

**HTTP Server:**

Serves web interface at `http://[device-ip]/`:
- Control page: Relay ON/OFF controls
- Settings page: Configuration editor
- Help page: User documentation
- Metrics endpoint: Prometheus-compatible telemetry at `/metrics`
- Status endpoint: JSON device status at `/status`

**SNMP Agent:**

Responds to SNMP queries on UDP port 161:
- System information (sysName, sysLocation, sysUptime)
- Network configuration (IP, subnet, gateway)
- Relay control (per-channel ON/OFF via SET commands)
- Power monitoring (voltage, current, power per channel)

**SNMP Trap Sender:**

Sends proactive notifications:
- Relay state changes
- Overcurrent events
- Link up/down transitions
- Configuration changes

**Error Handling:**

- Graceful boot without Ethernet: rest of system operates normally
- Hardware init failure: enters safe loop with heartbeat only
- Config load failure: applies factory defaults automatically
- Link supervision: handles intermittent connections robustly

### 6.8 ConsoleTask - UART Console

**Files:** `tasks/consoletask.h`, `tasks/consoletask.c`

UART console interface for device configuration, diagnostics, and firmware provisioning.

**Key Features:**
- Command-line interface via UART1 @ 115200 baud
- Multi-line command support (4 lines)
- Command history
- Provisioning mode for factory configuration
- Comprehensive diagnostic commands

**Initialization:**

```c
BaseType_t ConsoleTask_Init(bool enable);
bool Console_IsReady(void);
```

**Command Categories:**

**System Commands:**
- `help` - Show command list
- `version` - Show firmware version
- `uptime` - Show system uptime
- `reboot` - Soft reboot system
- `reset` - Hard reset via watchdog

**Configuration Commands:**
- `config show` - Display all configuration
- `config network` - Set network parameters
- `config name` - Set device name
- `config location` - Set device location

**Relay Commands:**
- `relay on <ch>` - Turn on channel
- `relay off <ch>` - Turn off channel
- `relay toggle <ch>` - Toggle channel
- `relay allon` - Turn all channels on
- `relay alloff` - Turn all channels off
- `relay status` - Show all relay states

**Power Monitoring Commands:**
- `meter show` - Display power measurements
- `meter refresh` - Force immediate measurement
- `meter cal zero` - Zero calibration
- `meter cal voltage <ref>` - Voltage calibration
- `meter cal current <ch> <ref>` - Current calibration

**Storage Commands:**
- `eeprom dump` - Dump EEPROM in hex
- `eeprom erase` - Full EEPROM erase
- `eeprom test` - Run self-test
- `factory reset` - Reset to factory defaults

**Diagnostic Commands:**
- `health` - Show system health metrics
- `tasks` - Show FreeRTOS task status
- `mem` - Show memory usage
- `crash` - Show crash log

**Provisioning Mode:**

Special mode for factory configuration:
```
> provision enter
Entering provisioning mode...

> provision set-mac 02:45:4E:00:00:01
MAC address set.

> provision set-region EU
Region set to EU (10A limit).

> provision save
Configuration saved. Rebooting...
```

### 6.9 LoggerTask - Centralized Logging

**Files:** `tasks/loggertask.h`, `tasks/loggertask.c`

Centralized logging subsystem with queue-based message handling.

**Key Features:**
- Queue-based logging (prevents blocking)
- USB-CDC output (UART0)
- Priority-based message handling
- Timestamp support
- Task name prefixing
- Force-print for critical messages

**Message Queue:**

```c
#define LOGGER_QUEUE_LEN 64
#define LOGGER_MSG_MAX 128

extern QueueHandle_t q_logger;
```

**Logging Functions:**

```c
// Standard logging (queued, non-blocking)
void log_printf(const char *fmt, ...);

// Force logging (bypasses queue for critical messages)
void log_printf_force(const char *fmt, ...);
```

**Log Levels:**

Controlled by compile-time flags in `config.h`:
```c
#define DEBUG 0       // Detailed debug info
#define INFO 1        // Informational messages
#define WARNING 1     // Warning conditions
#define ERROR 1       // Error conditions
```

**Usage Examples:**

```c
DEBUG_PRINT("Initializing driver at address 0x%02X\r\n", addr);
INFO_PRINT("Network link established\r\n");
WARNING_PRINT("Temperature above threshold: %.1f°C\r\n", temp);
ERROR_PRINT("EEPROM write failed at address 0x%04X\r\n", addr);
```

### 6.10 OCP Task - Overcurrent Protection

**Files:** `tasks/ocp.h`, `tasks/ocp.c`

Implements three-stage overcurrent protection state machine.

**Protection Stages:**

| State | Threshold | Action | Recovery |
|-------|-----------|--------|----------|
| **NORMAL** | Current < (Limit - 1.0A) | Normal operation | - |
| **WARNING** | Current ≥ (Limit - 1.0A) | Visual warning, relay switching allowed | Auto when current drops |
| **CRITICAL** | Current ≥ (Limit - 0.5A) | Audio/visual alarm, relay switching still allowed | Auto when current drops |
| **LOCKOUT** | Current ≥ Limit | **All relays OFF**, switching disabled | Current < (Limit - 1.5A) |

**Regional Limits:**

- **EU Region:** 10A total
- **US Region:** 15A total

**Thresholds (configurable in `config.h`):**

```c
#define ENERGIS_CURRENT_SAFETY_MARGIN_A 0.5f    // CRITICAL threshold offset
#define ENERGIS_CURRENT_WARNING_OFFSET_A 1.0f   // WARNING threshold offset
#define ENERGIS_CURRENT_RECOVERY_OFFSET_A 1.5f  // RECOVERY threshold offset
```

**State Machine:**

```
        NORMAL
           │
           v (current ≥ LIMIT - 1.0A)
        WARNING
           │
           v (current ≥ LIMIT - 0.5A)
        CRITICAL
           │
           v (current ≥ LIMIT)
        LOCKOUT ──[trip]──> All Relays OFF
           │
           └──> RECOVERY (current < LIMIT - 1.5A)
```

**API:**

```c
// Check if relay switching is allowed
bool allowed = Overcurrent_IsSwitchingAllowed();

// Get current protection state
ocp_state_t state = Overcurrent_GetState();

// Get current total current
float current = Overcurrent_GetTotalCurrent();

// Get regional limit
float limit = Overcurrent_GetLimit();
```

**Integration:**

OCP Task continuously monitors total current from MeterTask:
1. MeterTask updates `hlw8032_get_total_current()` every 40ms
2. OCP Task reads total current each cycle
3. Evaluates state machine transitions
4. If LOCKOUT triggered: sends `Switch_AllOff()` command
5. SwitchTask queries `Overcurrent_IsSwitchingAllowed()` before relay ON

**Visual Feedback:**

- **NORMAL:** No indication
- **WARNING:** Fault LED slow blink (1 Hz)
- **CRITICAL:** Fault LED fast blink (4 Hz)
- **LOCKOUT:** Fault LED solid ON

**Logging:**

All state transitions logged with error codes:
```c
ERROR_PRINT_CODE(errorcode, "[OCP] LOCKOUT triggered at %.2fA (limit %.2fA)\r\n",
                 total_current, limit);
```

---

## 7. Storage Subsystem

The storage subsystem is organized into modular components, each managing a specific section of the CAT24C256 EEPROM.

### 7.1 EEPROM Memory Map

**File:** `misc/eeprom_memorymap.h`

Defines fixed-offset sections and data structures for all persistent configuration.

**Memory Layout:**

| Address Range | Section | Size | Description |
|---------------|---------|------|-------------|
| 0x0000-0x001F | Factory Marker | 32 bytes | First-boot detection |
| 0x0020-0x005F | Network Config | 64 bytes | IP, MAC, DHCP mode |
| 0x0060-0x009F | User Preferences | 64 bytes | Name, location, settings |
| 0x00A0-0x00A7 | Relay Startup States | 8 bytes | Power-on relay states |
| 0x00A8-0x02A7 | User Output Presets | 512 bytes | 5 presets × 100 bytes |
| 0x02A8-0x04A7 | Channel Labels | 512 bytes | 8 channels × 64 chars |
| 0x04A8-0x06A7 | Sensor Calibration | 512 bytes | 8 channels × 64 bytes |
| 0x06A8-0x16A7 | Energy Monitor Log | 4096 bytes | Ring buffer |
| 0x16A8-0x36A7 | Error Event Log | 8192 bytes | Ring buffer |
| 0x36A8-0x56A7 | Warning Event Log | 8192 bytes | Ring buffer |
| 0x56A8-0x7FFF | Reserved | 10072 bytes | Future expansion |

**Data Structures:**

```c
// Network configuration (64 bytes)
typedef struct {
    uint8_t mac[6];                  // MAC address
    uint8_t ip[4];                   // IP address
    uint8_t sn[4];                   // Subnet mask
    uint8_t gw[4];                   // Gateway
    uint8_t dns[4];                  // DNS server
    uint8_t dhcp;                    // DHCP mode (0=static, 1=DHCP)
    uint8_t reserved[39];            // Reserved
    uint16_t crc;                    // CRC-16
} __attribute__((packed)) networkInfo;

// User preferences (64 bytes)
typedef struct {
    char device_name[32];            // Device name
    char location[24];               // Physical location
    uint8_t display_brightness;      // Display brightness (0-100)
    uint8_t reserved[5];             // Reserved
    uint16_t crc;                    // CRC-16
} __attribute__((packed)) userPrefInfo;

// Sensor calibration (64 bytes per channel)
typedef struct {
    uint8_t calibrated;              // 0xCA if calibrated
    float voltage_factor;            // Voltage multiplier
    float voltage_offset;            // Voltage offset [V]
    float current_factor;            // Current multiplier
    float current_offset;            // Current offset [A]
    float resistor_value;            // Shunt resistor [Ω]
    uint8_t reserved[39];            // Reserved
    uint16_t crc;                    // CRC-16
} __attribute__((packed)) hlw_calib_t;
```

### 7.2 Factory Defaults Subsystem

**Files:** `tasks/storage_submodule/factory_defaults.h`, `tasks/storage_submodule/factory_defaults.c`

Manages first-boot detection and default configuration initialization.

**Factory Marker:**

```c
#define FACTORY_MARKER "ENERGIS_FACTORY_V1"  // 18 bytes
```

Stored at address 0x0000. If marker is missing or corrupted, system performs first-boot initialization.

**First-Boot Sequence:**

1. Check factory marker validity
2. If invalid:
   - Write default network configuration
   - Write default user preferences
   - Initialize all relay states to OFF
   - Clear all event logs
   - Write factory marker
3. Set CFG_READY event flag

**Default Configuration:**

```c
// Default network config
networkInfo default_net = {
    .mac = {0x02, 0x45, 0x4E, 0x00, 0x00, 0x01},  // Locally-administered OUI
    .ip = {192, 168, 0, 22},
    .sn = {255, 255, 255, 0},
    .gw = {192, 168, 0, 1},
    .dns = {8, 8, 8, 8},
    .dhcp = EEPROM_NETINFO_STATIC
};

// Default preferences
userPrefInfo default_prefs = {
    .device_name = "ENERGIS-PDU",
    .location = "Server Rack",
    .display_brightness = 75
};
```

**API:**

```c
// Check if first boot
bool is_first_boot = FactoryDefaults_IsFirstBoot();

// Initialize defaults
bool ok = FactoryDefaults_Initialize();

// Write factory marker
bool ok = FactoryDefaults_WriteMarker();
```

### 7.3 User Output Subsystem

**Files:** `tasks/storage_submodule/user_output.h`, `tasks/storage_submodule/user_output.c`

Manages relay state persistence and preset system.

**Features:**
- Startup relay configuration (applied at boot)
- 5 user-defined presets
- Preset names (up to 31 characters)
- 8-bit bitmask per preset (bit N = channel N)

**Preset Structure:**

```c
typedef struct {
    uint8_t valid;           // 0xA5 if valid
    char name[32];           // Preset name
    uint8_t relay_mask;      // Relay states (bit N = channel N)
    uint8_t reserved[63];    // Reserved
    uint16_t crc;            // CRC-16
} __attribute__((packed)) user_output_preset_t;
```

**API:**

```c
// Get startup relay states
uint8_t states[8];
bool ok = UserOutput_GetStartupStates(states);

// Set startup relay states
uint8_t new_states[8] = {1, 1, 0, 0, 1, 1, 0, 0};
bool ok = UserOutput_SetStartupStates(new_states);

// Save preset
bool ok = UserOutput_SavePreset(uint8_t index, "Office Hours", 0b11111100);

// Load preset
char name[32];
uint8_t mask;
bool ok = UserOutput_LoadPreset(uint8_t index, name, &mask);

// Delete preset
bool ok = UserOutput_DeletePreset(uint8_t index);

// Set startup preset index
bool ok = UserOutput_SetStartupPresetIndex(uint8_t index);

// Get startup preset index
uint8_t index = UserOutput_GetStartupPresetIndex();  // 0xFF if none
```

### 7.4 Network Configuration Subsystem

**Files:** `tasks/storage_submodule/network.h`, `tasks/storage_submodule/network.c`

Manages network configuration with CRC validation and fallback defaults.

**Configuration Fields:**
- MAC address (6 bytes)
- IP address (4 bytes)
- Subnet mask (4 bytes)
- Gateway (4 bytes)
- DNS server (4 bytes)
- DHCP mode (0=static, 1=DHCP)
- CRC-16 checksum

**API:**

```c
// Read network config from EEPROM
networkInfo net;
bool ok = Network_Read(&net);

// Write network config to EEPROM
networkInfo new_net = { /* ... */ };
bool ok = Network_Write(&new_net);

// Validate CRC
bool valid = Network_ValidateCRC(&net);

// Compute CRC for structure
uint16_t crc = Network_ComputeCRC(&net);
```

**Fallback Defaults:**

If EEPROM read fails or CRC invalid:
1. Apply factory default network configuration
2. Log warning
3. Continue operation (non-fatal)

### 7.5 Calibration Subsystem

**Files:** `tasks/storage_submodule/calibration.h`, `tasks/storage_submodule/calibration.c`

Manages per-channel HLW8032 calibration coefficients.

**Calibration Parameters:**
- Voltage factor and offset
- Current factor and offset
- Shunt resistor value
- Calibration validity flag (0xCA = calibrated)

**API:**

```c
// Read calibration for channel
hlw_calib_t calib;
bool ok = Calibration_Read(uint8_t channel, &calib);

// Write calibration for channel
hlw_calib_t new_calib = {
    .calibrated = 0xCA,
    .voltage_factor = 1.05,
    .voltage_offset = -0.5,
    .current_factor = 1.02,
    .current_offset = 0.0,
    .resistor_value = 0.001
};
bool ok = Calibration_Write(uint8_t channel, &new_calib);

// Check if channel is calibrated
bool calibrated = Calibration_IsCalibrated(uint8_t channel);

// Reset channel to defaults
bool ok = Calibration_ResetToDefaults(uint8_t channel);
```

**Calibration Workflow:**

1. **Zero Calibration (0V, 0A):**
   ```c
   hlw8032_calibration_start_zero_all();
   ```
   Measures voltage and current offsets with all channels OFF.

2. **Voltage Calibration (known voltage):**
   ```c
   hlw8032_calibration_start_voltage_all(230.0);  // 230V reference
   ```
   Computes voltage gain factor for each channel.

3. **Current Calibration (known load):**
   ```c
   hlw8032_calibration_start_current_single(channel, 5.0);  // 5A reference
   ```
   Computes current gain factor for specified channel.

4. **Save to EEPROM:**
   ```c
   hlw_calib_t calib;
   hlw8032_get_calibration(channel, &calib);
   storage_set_sensor_cal(channel, &calib);
   storage_commit_now(5000);
   ```

### 7.6 Event Log Subsystem

**Files:** `tasks/storage_submodule/event_log.h`, `tasks/storage_submodule/event_log.c`

Implements dual ring buffers for error and warning event history.

**Ring Buffer Structure:**

```c
typedef struct {
    uint16_t head;           // Write pointer
    uint16_t tail;           // Read pointer (not used in append-only mode)
    uint16_t count;          // Number of entries
    uint16_t max_entries;    // Maximum capacity
} event_log_header_t;
```

**Event Entry:**

```c
typedef struct {
    uint32_t timestamp;      // Seconds since boot
    uint16_t error_code;     // 16-bit error code
    uint8_t reserved[10];    // Reserved
} __attribute__((packed)) event_entry_t;
```

**API:**

```c
// Append error event
bool ok = EventLog_AppendError(uint16_t error_code, uint32_t timestamp);

// Append warning event
bool ok = EventLog_AppendWarning(uint16_t warning_code, uint32_t timestamp);

// Read error log entries
event_entry_t entries[100];
uint16_t count = EventLog_ReadErrors(entries, 100);

// Read warning log entries
uint16_t count = EventLog_ReadWarnings(entries, 100);

// Clear error log
bool ok = EventLog_ClearErrors();

// Clear warning log
bool ok = EventLog_ClearWarnings();
```

**Ring Buffer Behavior:**

- **Append-only:** New entries always written at head
- **Wraparound:** Head wraps to 0 when reaching end
- **Overflow:** Oldest entries overwritten when full
- **Persistence:** Ring buffer state persisted to EEPROM

**Deferred Logging:**

To prevent deadlocks during EEPROM access:
```c
// From any task (non-blocking)
Storage_EnqueueErrorCode(error_code);
Storage_EnqueueWarningCode(warning_code);

// StorageTask processes queue and writes to EEPROM
```

### 7.7 Energy Monitor Subsystem

**Files:** `tasks/storage_submodule/energy_monitor.h`, `tasks/storage_submodule/energy_monitor.c`

Ring buffer for historical energy consumption tracking.

**Entry Structure:**

```c
typedef struct {
    uint32_t timestamp;      // Unix timestamp or uptime
    uint8_t channel;         // Channel index
    float energy_kwh;        // Accumulated energy [kWh]
    uint8_t reserved[7];     // Reserved
} __attribute__((packed)) energy_entry_t;
```

**API:**

```c
// Append energy snapshot
bool ok = EnergyMonitor_AppendEntry(uint8_t channel, float energy_kwh, uint32_t timestamp);

// Read entries
energy_entry_t entries[100];
uint16_t count = EnergyMonitor_ReadEntries(entries, 100);

// Clear log
bool ok = EnergyMonitor_Clear();
```

### 7.8 User Preferences Subsystem

**Files:** `tasks/storage_submodule/user_prefs.h`, `tasks/storage_submodule/user_prefs.c`

Manages device identification and display settings.

**Preference Fields:**
- Device name (32 characters)
- Physical location (24 characters)
- Display brightness (0-100%)

**API:**

```c
// Read preferences
userPrefInfo prefs;
bool ok = UserPrefs_Read(&prefs);

// Write preferences
userPrefInfo new_prefs = {
    .device_name = "ENERGIS-PDU-001",
    .location = "Rack 42, Position U12",
    .display_brightness = 80
};
bool ok = UserPrefs_Write(&new_prefs);
```

### 7.9 Channel Labels Subsystem

**Files:** `tasks/storage_submodule/channel_labels.h`, `tasks/storage_submodule/channel_labels.c`

RAM-cached channel naming with lazy loading.

**Features:**
- 8 channels × 64 characters
- UTF-8 support
- Lazy loading (load on first access)
- Write-through cache

**Cache Structure:**

```c
static struct {
    char labels[8][65];      // 64 chars + null terminator
    bool loaded[8];          // Per-channel load flags
    bool cache_valid;        // Global cache validity
} label_cache;
```

**API:**

```c
// Get label (from cache, lazy load if needed)
char label[65];
bool ok = ChannelLabels_Get(uint8_t channel, label, sizeof(label));

// Set label (write-through: cache + EEPROM)
bool ok = ChannelLabels_Set(uint8_t channel, "Web Server");

// Get all labels at once
char all_labels[8][65];
bool ok = ChannelLabels_GetAll(all_labels, 65);

// Invalidate cache (force reload)
void ChannelLabels_InvalidateCache(void);
```

**Lazy Loading:**

First access to a channel triggers EEPROM read:
```c
if (!label_cache.loaded[channel]) {
    // Read from EEPROM
    CAT24C256_ReadBuffer(EEPROM_CHANNEL_LABELS_START + (channel * 64),
                         label_cache.labels[channel], 64);
    label_cache.loaded[channel] = true;
}
```

### 7.10 Device Identity Subsystem

**Files:** `tasks/storage_submodule/device_identity.h`, `tasks/storage_submodule/device_identity.c`

Manages device-specific identification and MAC address derivation.

**Identity Fields:**
- Serial number (16 characters)
- Hardware revision (3 bytes: major.minor.patch)
- Manufacturing date (4 bytes: year, month, day, batch)
- Region code (1 byte: EU/US)

**MAC Address Derivation:**

MAC addresses are derived from serial number using locally-administered OUI:
```
MAC = 02:45:4E:[SN_HASH:24]
      │  │  │  └─ 24-bit hash of serial number
      │  │  └─ 'N' (0x4E)
      │  └─ 'E' (0x45)
      └─ Locally-administered, unicast (0x02)
```

**API:**

```c
// Get device identity
device_identity_t identity;
bool ok = DeviceIdentity_Read(&identity);

// Derive MAC from serial number
uint8_t mac[6];
DeviceIdentity_DeriveMACFromSerial("ENERGIS001234567", mac);
// Result: 02:45:4E:XX:YY:ZZ

// Get region code
uint8_t region = DeviceIdentity_GetRegion();  // 0=EU, 1=US

// Get current limit based on region
float limit = DeviceIdentity_GetCurrentLimit();  // 10.0A (EU) or 15.0A (US)
```

---

## 8. Network Services

### 8.1 HTTP Server

**Files:** `web_handlers/http_server.h`, `web_handlers/http_server.c`

Lightweight HTTP/1.0 server implementation for W5500 socket 0.

**Supported Features:**
- HTTP/1.0 protocol
- GET and POST methods
- URL-encoded form data
- Static HTML pages (gzip-compressed)
- JSON API endpoints
- Prometheus metrics endpoint

**Architecture:**

```
W5500 Socket 0 (TCP port 80)
     │
     v
HTTP Parser (request line, headers, body)
     │
     v
Route Dispatcher
     ├─> / ────────────> Control page handler
     ├─> /settings ───> Settings page handler
     ├─> /help ───────> Help page handler
     ├─> /status ─────> Status JSON endpoint
     ├─> /metrics ────> Prometheus metrics
     └─> /preset ─────> Preset management
```

**HTTP Request Handling:**

```c
// Main server loop (called by NetTask)
void http_server_process(uint8_t socket_num) {
    // 1. Check socket status
    uint8_t status = getSn_SR(socket_num);

    // 2. Accept new connections
    if (status == SOCK_LISTEN) {
        // Wait for client
    }

    // 3. Receive request
    if (status == SOCK_ESTABLISHED) {
        uint16_t rx_avail = getSn_RX_RSR(socket_num);
        if (rx_avail > 0) {
            // Read request, parse, dispatch to handler
            http_handle_request(socket_num);
        }
    }

    // 4. Close connection
    if (status == SOCK_CLOSE_WAIT) {
        disconnect(socket_num);
    }
}
```

**Route Handlers:**

Each route has a dedicated handler function:

```c
// Control page (relay ON/OFF controls)
void handle_control_page(uint8_t socket);

// Settings page (network config, device name, etc.)
void handle_settings_page(uint8_t socket);

// Help page (user documentation)
void handle_help_page(uint8_t socket);

// Status JSON endpoint
void handle_status_json(uint8_t socket);

// Metrics endpoint (Prometheus format)
void handle_metrics(uint8_t socket);

// Preset management
void handle_preset_api(uint8_t socket, http_request_t *req);
```

**Response Construction:**

```c
// Helper functions for building responses
void http_send_response(uint8_t socket, uint16_t status_code,
                       const char *content_type, const char *body, size_t body_len);

void http_send_html(uint8_t socket, const char *html);
void http_send_json(uint8_t socket, const char *json);
void http_send_error(uint8_t socket, uint16_t status_code, const char *message);
```

**Static Content Serving:**

HTML pages are pre-compressed with gzip and embedded as C arrays:

```c
// control.html.gz embedded as control_gz.h
extern const uint8_t control_html_gz[];
extern const size_t control_html_gz_len;

void handle_control_page(uint8_t socket) {
    http_send_header(socket, 200, "text/html", control_html_gz_len,
                     "gzip", NULL);
    http_send_body(socket, control_html_gz, control_html_gz_len);
}
```

**POST Request Handling:**

Form data parsed from request body:

```c
// Parse URL-encoded form data
void http_parse_form_data(const char *body, form_param_t *params, uint8_t max_params);

// Example: relay control
void handle_control_post(uint8_t socket, http_request_t *req) {
    form_param_t params[8];
    http_parse_form_data(req->body, params, 8);

    for (int i = 0; i < 8; i++) {
        if (strcmp(params[i].name, "relay_0") == 0) {
            bool state = (strcmp(params[i].value, "on") == 0);
            Switch_SetChannel(0, state);
        }
    }

    // Send redirect response
    http_send_redirect(socket, "/");
}
```

### 8.2 SNMP Agent

**Files:** `drivers/snmp.h`, `drivers/snmp.c`, `snmp/snmp_*.c`

SNMP v1/v2c agent implementation for W5500 socket 1.

**Supported PDUs:**
- GetRequest (0xA0)
- GetNextRequest (0xA1)
- SetRequest (0xA3)

**MIB Structure:**

```
1.3.6.1.4.1.99999 (Enterprise OID)
    └─> .1 (System)
        ├─> .1.1 sysName (device name)
        ├─> .1.2 sysLocation (physical location)
        └─> .1.3 sysUptime (seconds since boot)
    └─> .2 (Network)
        ├─> .2.1 ipAddress (IP address)
        ├─> .2.2 subnetMask (subnet mask)
        ├─> .2.3 gateway (gateway address)
        └─> .2.4 dhcpMode (0=static, 1=DHCP)
    └─> .3 (Outlet Control)
        ├─> .3.1.N outletState (0=OFF, 1=ON)
        └─> .3.2.N outletName (channel label)
    └─> .4 (Power Monitoring)
        ├─> .4.1.N voltage (RMS voltage × 10)
        ├─> .4.2.N current (RMS current × 100)
        ├─> .4.3.N power (active power × 1)
        └─> .4.4.N energy (accumulated kWh × 100)
    └─> .5 (Voltage Monitoring)
        ├─> .5.1 dieTemp (RP2040 temp × 10)
        ├─> .5.2 vusb (USB voltage × 100)
        └─> .5.3 vsupply (12V supply × 100)
```

**OID Handlers:**

Each MIB section has a handler module:

```c
// System info
void snmp_handle_system(snmp_pdu_t *pdu);

// Network config
void snmp_handle_network(snmp_pdu_t *pdu);

// Outlet control
void snmp_handle_outlet_ctrl(snmp_pdu_t *pdu);

// Power monitoring
void snmp_handle_power_mon(snmp_pdu_t *pdu);

// Voltage monitoring
void snmp_handle_voltage_mon(snmp_pdu_t *pdu);
```

**SNMP Processing:**

```c
// Main SNMP loop (called by NetTask)
void snmp_process(uint8_t socket_num) {
    // 1. Check for incoming UDP packets
    uint16_t rx_avail = getSn_RX_RSR(socket_num);
    if (rx_avail == 0) return;

    // 2. Receive packet
    uint8_t buf[512];
    uint16_t len = recvfrom(socket_num, buf, sizeof(buf), peer_ip, &peer_port);

    // 3. Parse SNMP message
    snmp_message_t msg;
    if (!snmp_parse_message(buf, len, &msg)) {
        return;  // Invalid message
    }

    // 4. Verify community string
    if (strcmp(msg.community, "public") != 0) {
        return;  // Access denied
    }

    // 5. Dispatch PDU to handler
    snmp_response_t response;
    snmp_handle_pdu(&msg.pdu, &response);

    // 6. Encode and send response
    uint8_t response_buf[512];
    uint16_t response_len = snmp_encode_message(&response, response_buf);
    sendto(socket_num, response_buf, response_len, peer_ip, peer_port);
}
```

**SET Request Example:**

Turn on outlet 3 via SNMP:
```bash
snmpset -v2c -c private 192.168.0.22 1.3.6.1.4.1.99999.3.1.3 i 1
```

Handler:
```c
void snmp_handle_outlet_ctrl_set(snmp_pdu_t *pdu, snmp_response_t *response) {
    // Extract channel from OID
    uint8_t channel = pdu->oid.value[pdu->oid.len - 1] - 1;

    // Extract new state from value
    bool state = (pdu->value.integer == 1);

    // Apply to hardware
    switch_result_t result = Switch_SetChannel(channel, state);

    if (result == SWITCH_OK) {
        response->error_status = SNMP_ERR_NO_ERROR;
    } else if (result == SWITCH_ERR_OVERCURRENT) {
        response->error_status = SNMP_ERR_READ_ONLY;  // Reject due to OCP
    } else {
        response->error_status = SNMP_ERR_GEN_ERR;
    }
}
```

### 8.3 SNMP Trap Sender

**Files:** `snmp/snmp_trap.c`

Sends proactive SNMP trap notifications to configured trap receiver.

**Trap Types:**

```c
typedef enum {
    TRAP_RELAY_STATE_CHANGE,     // Relay turned ON/OFF
    TRAP_OVERCURRENT_WARNING,    // Current in WARNING zone
    TRAP_OVERCURRENT_CRITICAL,   // Current in CRITICAL zone
    TRAP_OVERCURRENT_LOCKOUT,    // OCP lockout triggered
    TRAP_LINK_UP,                // Ethernet link established
    TRAP_LINK_DOWN,              // Ethernet link lost
    TRAP_CONFIG_CHANGE           // Configuration modified
} trap_type_t;
```

**Trap Sending:**

```c
// Send trap notification
void snmp_send_trap(trap_type_t trap_type, uint8_t channel, float value) {
    // 1. Build trap PDU
    snmp_trap_pdu_t trap = {
        .enterprise_oid = {1, 3, 6, 1, 4, 1, 99999},
        .agent_addr = {192, 168, 0, 22},
        .generic_trap = 6,  // Enterprise-specific
        .specific_trap = trap_type,
        .timestamp = Health_GetUptimeSeconds() * 100,  // Timeticks
    };

    // 2. Add variable bindings
    snmp_add_varbind(&trap, oid, value);

    // 3. Encode message
    uint8_t buf[512];
    uint16_t len = snmp_encode_trap(&trap, buf, sizeof(buf));

    // 4. Send to trap receiver
    sendto(SNMP_TRAP_SOCKET, buf, len, trap_receiver_ip, 162);
}
```

**Example Usage:**

```c
// In OCP Task: send trap when entering LOCKOUT
if (state == OCP_STATE_LOCKOUT) {
    snmp_send_trap(TRAP_OVERCURRENT_LOCKOUT, 0xFF, total_current);
}

// In SwitchTask: send trap on relay state change
if (result == SWITCH_OK) {
    snmp_send_trap(TRAP_RELAY_STATE_CHANGE, channel, state ? 1.0 : 0.0);
}
```

### 8.4 Metrics Endpoint (Prometheus)

**Files:** `web_handlers/metrics_handler.h`, `web_handlers/metrics_handler.c`

Prometheus-compatible metrics endpoint at `/metrics`.

**Metric Types:**

```
# HELP energis_outlet_voltage_volts RMS voltage per outlet
# TYPE energis_outlet_voltage_volts gauge
energis_outlet_voltage_volts{outlet="0",label="Web Server"} 230.2

# HELP energis_outlet_current_amps RMS current per outlet
# TYPE energis_outlet_current_amps gauge
energis_outlet_current_amps{outlet="0",label="Web Server"} 1.23

# HELP energis_outlet_power_watts Active power per outlet
# TYPE energis_outlet_power_watts gauge
energis_outlet_power_watts{outlet="0",label="Web Server"} 283.0

# HELP energis_outlet_energy_kwh Accumulated energy per outlet
# TYPE energis_outlet_energy_kwh counter
energis_outlet_energy_kwh{outlet="0",label="Web Server"} 12.456

# HELP energis_outlet_uptime_seconds Outlet ON-time
# TYPE energis_outlet_uptime_seconds counter
energis_outlet_uptime_seconds{outlet="0",label="Web Server"} 86400

# HELP energis_outlet_state Outlet relay state (0=OFF, 1=ON)
# TYPE energis_outlet_state gauge
energis_outlet_state{outlet="0",label="Web Server"} 1

# HELP energis_total_current_amps Total current across all outlets
# TYPE energis_total_current_amps gauge
energis_total_current_amps 8.45

# HELP energis_die_temperature_celsius RP2040 die temperature
# TYPE energis_die_temperature_celsius gauge
energis_die_temperature_celsius 42.3

# HELP energis_vusb_volts USB bus voltage
# TYPE energis_vusb_volts gauge
energis_vusb_volts 5.12

# HELP energis_vsupply_volts 12V supply voltage
# TYPE energis_vsupply_volts gauge
energis_vsupply_volts 12.05

# HELP energis_uptime_seconds System uptime
# TYPE energis_uptime_seconds counter
energis_uptime_seconds 86400
```

**Handler Implementation:**

```c
void handle_metrics(uint8_t socket) {
    // 1. Send HTTP header
    http_send_header(socket, 200, "text/plain; version=0.0.4", 0, NULL, NULL);

    // 2. Send metrics for each outlet
    for (uint8_t ch = 0; ch < 8; ch++) {
        meter_telemetry_t telem;
        if (MeterTask_GetTelemetry(ch, &telem)) {
            char label[65];
            storage_get_channel_label(ch, label, sizeof(label));

            http_send_metric(socket, "energis_outlet_voltage_volts",
                           ch, label, telem.voltage);
            http_send_metric(socket, "energis_outlet_current_amps",
                           ch, label, telem.current);
            http_send_metric(socket, "energis_outlet_power_watts",
                           ch, label, telem.power);
            http_send_metric(socket, "energis_outlet_energy_kwh",
                           ch, label, telem.energy_kwh);
            http_send_metric(socket, "energis_outlet_uptime_seconds",
                           ch, label, telem.uptime);
            http_send_metric(socket, "energis_outlet_state",
                           ch, label, telem.relay_state ? 1.0 : 0.0);
        }
    }

    // 3. Send system metrics
    system_telemetry_t sys;
    if (MeterTask_GetSystemTelemetry(&sys)) {
        http_send_metric(socket, "energis_die_temperature_celsius",
                       0xFF, NULL, sys.die_temp_c);
        http_send_metric(socket, "energis_vusb_volts",
                       0xFF, NULL, sys.vusb_volts);
        http_send_metric(socket, "energis_vsupply_volts",
                       0xFF, NULL, sys.vsupply_volts);
    }

    // 4. Send uptime
    uint32_t uptime = Health_GetUptimeSeconds();
    http_send_metric(socket, "energis_uptime_seconds",
                   0xFF, NULL, uptime);

    // 5. Close connection
    disconnect(socket);
}
```

**Prometheus Configuration:**

```yaml
scrape_configs:
  - job_name: 'energis-pdu'
    static_configs:
      - targets: ['192.168.0.22:80']
    metrics_path: '/metrics'
    scrape_interval: 10s
```

---

## 9. Safety and Protection

### 9.1 Watchdog System

The ENERGIS PDU implements a comprehensive watchdog system to ensure system reliability and automatic recovery from faults.

**Architecture:**

```
RP2040 Hardware Watchdog (2-second timeout)
     ↑
HealthTask (feeds every 500ms)
     ↑
Task Heartbeats (all tasks report periodically)
```

**Watchdog Configuration:**

```c
#define HEALTH_WATCHDOG_TIMEOUT_MS 2000  // 2-second watchdog
#define HEALTH_HEARTBEAT_INTERVAL_MS 500 // Feed every 500ms
```

**Watchdog Reset Reasons:**

The system detects and logs reset reasons:

```c
typedef enum {
    REBOOT_REASON_POWER_ON,      // Clean power-on reset
    REBOOT_REASON_WATCHDOG,      // Watchdog timeout (hung task)
    REBOOT_REASON_CRASH,         // Software crash detected
    REBOOT_REASON_DEBUGGER,      // Debugger reset
    REBOOT_REASON_UNKNOWN        // Unable to determine
} reboot_reason_t;
```

**Recovery Actions:**

On watchdog reset:
1. System reboots
2. Crash log written to EEPROM (if applicable)
3. Reboot reason logged
4. System reinitializes with last known good configuration

### 9.2 Crash Detection and Logging

**Files:** `misc/crashlog.h`, `misc/crashlog.c`

Detects and logs software crashes for post-mortem analysis.

**Crash Detection:**

Uses RP2040 scratch registers to detect abnormal reboots:

```c
// Early boot snapshot (before main)
void Helpers_EarlyBootSnapshot(void) {
    uint32_t magic = watchdog_hw->scratch[0];
    if (magic == CRASH_MAGIC) {
        // Crash detected - system did not shut down cleanly
        Crashlog_Capture();
    }
    watchdog_hw->scratch[0] = CRASH_MAGIC;  // Set magic for next boot
}

// Clean shutdown
void system_shutdown(void) {
    watchdog_hw->scratch[0] = CLEAN_SHUTDOWN_MAGIC;
    // Trigger reboot
}
```

**Crash Log Entry:**

```c
typedef struct {
    uint32_t timestamp;          // Uptime at crash
    reboot_reason_t reason;      // Reset reason
    uint32_t stack_pointer;      // SP at crash
    uint32_t link_register;      // LR at crash
    uint32_t program_counter;    // PC at crash
    char task_name[16];          // Active task
    uint8_t reserved[28];        // Reserved
} __attribute__((packed)) crash_entry_t;
```

**API:**

```c
// Capture crash information
void Crashlog_Capture(void);

// Get last crash entry
crash_entry_t crash;
bool ok = Crashlog_GetLast(&crash);

// Clear crash log
void Crashlog_Clear(void);

// Dump crash log to console
void Crashlog_Dump(void);
```

### 9.3 Overcurrent Protection State Machine

**Files:** `tasks/ocp.h`, `tasks/ocp.c`

Three-stage protection with automatic trip and recovery.

**State Machine Diagram:**

```
                    ┌─────────────────┐
                    │     NORMAL      │
                    │  Current < L-1  │
                    └────────┬────────┘
                             │
                    Current ≥ L-1
                             │
                             v
                    ┌─────────────────┐
                    │    WARNING      │
                    │  L-1 ≤ I < L-0.5│
                    └────────┬────────┘
                             │
                    Current ≥ L-0.5
                             │
                             v
                    ┌─────────────────┐
                    │    CRITICAL     │
                    │  L-0.5 ≤ I < L  │
                    └────────┬────────┘
                             │
                      Current ≥ L
                             │
                             v
                    ┌─────────────────┐
                    │    LOCKOUT      │
                    │   All OFF       │
                    │ Switching Blocked│
                    └────────┬────────┘
                             │
                    Current < L-1.5
                             │
                             v
                    ┌─────────────────┐
                    │    RECOVERY     │
                    │  Wait for clear │
                    └─────────────────┘
```

Where:
- **L** = Regional current limit (10A EU, 15A US)
- **L-1** = WARNING threshold (9A EU, 14A US)
- **L-0.5** = CRITICAL threshold (9.5A EU, 14.5A US)
- **L-1.5** = RECOVERY threshold (8.5A EU, 13.5A US)

**State Behaviors:**

| State | Relay Switching | Visual Indicator | Audio Alarm |
|-------|----------------|------------------|-------------|
| NORMAL | Allowed | None | Off |
| WARNING | Allowed | Fault LED slow blink (1 Hz) | Off |
| CRITICAL | Allowed | Fault LED fast blink (4 Hz) | On |
| LOCKOUT | **Blocked** | Fault LED solid ON | On |
| RECOVERY | Blocked | Fault LED slow blink (1 Hz) | Off |

**Integration with SwitchTask:**

```c
// SwitchTask checks before turning ON relay
switch_result_t Switch_SetChannel(uint8_t channel, bool state) {
    if (state == true) {  // Turning ON
        if (!Overcurrent_IsSwitchingAllowed()) {
            return SWITCH_ERR_OVERCURRENT;  // Rejected
        }
    }
    // Proceed with relay control
}
```

**Trip Action:**

When LOCKOUT triggered:
1. Send `Switch_AllOff()` command
2. Log error code to EEPROM
3. Send SNMP trap notification
4. Block all relay ON operations until recovery

**Recovery:**

System automatically exits LOCKOUT when total current drops below recovery threshold:
```c
if (state == OCP_STATE_LOCKOUT && total_current < (limit - 1.5)) {
    state = OCP_STATE_RECOVERY;
    INFO_PRINT("[OCP] Exiting lockout - current safe\r\n");
}
```

Manual relay control allowed after recovery.

### 9.4 Hardware Verification

All relay operations include read-back verification:

```c
switch_result_t Switch_SetChannel(uint8_t channel, bool state) {
    // 1. Write to relay MCP
    mcp_write_pin(mcp_relay(), channel, state);

    // 2. Read back from hardware
    uint8_t readback = mcp_read_pin(mcp_relay(), channel);

    // 3. Verify match
    if (readback != state) {
        ERROR_PRINT("[Switch] Verification failed on channel %d\r\n", channel);
        return SWITCH_ERR_VERIFY_FAIL;
    }

    // 4. Update cache only if verified
    relay_cache |= (state << channel);
    return SWITCH_OK;
}
```

If verification fails:
- Retry with exponential backoff (3 attempts)
- Log error code to EEPROM
- Return error to caller
- Do not update cache

---

## 10. Configuration and Calibration

### 10.1 Network Configuration

**Supported Modes:**
- Static IP
- DHCP (W5500 hardware DHCP)

**Configuration Fields:**
- MAC address (6 bytes, locally-administered OUI)
- IP address (4 bytes)
- Subnet mask (4 bytes)
- Gateway (4 bytes)
- DNS server (4 bytes)
- DHCP mode (0=static, 1=DHCP)

**Factory Defaults:**

```c
MAC: 02:45:4E:XX:XX:XX (derived from serial number)
IP: 192.168.0.22
Subnet: 255.255.255.0
Gateway: 192.168.0.1
DNS: 8.8.8.8
Mode: Static
```

**Configuration via HTTP:**

Navigate to `http://[device-ip]/settings` and modify network settings. Submit form to apply changes. Device reboots automatically to apply new network configuration.

**Configuration via SNMP:**

```bash
# Set IP address
snmpset -v2c -c private 192.168.0.22 1.3.6.1.4.1.99999.2.1 s "192.168.1.100"

# Set DHCP mode
snmpset -v2c -c private 192.168.0.22 1.3.6.1.4.1.99999.2.4 i 1
```

**Configuration via Console:**

```
> config network ip 192.168.1.100
IP address set to 192.168.1.100

> config network dhcp enable
DHCP mode enabled

> config network save
Configuration saved. Reboot required to apply changes.

> reboot
Rebooting...
```

### 10.2 Sensor Calibration

**Calibration Types:**

1. **Zero Calibration (0V, 0A):** Measures voltage and current offsets
2. **Voltage Calibration:** Computes voltage gain factor
3. **Current Calibration:** Computes current gain factor

**Zero Calibration Procedure:**

```
1. Turn OFF all relay channels
2. Disconnect all loads
3. Run console command:
   > meter cal zero
   Starting zero calibration...
   Sampling channel 0... done
   Sampling channel 1... done
   ...
   Sampling channel 7... done
   Zero calibration complete.
   Offsets saved to EEPROM.
```

**Voltage Calibration Procedure:**

```
1. Connect all channels to known voltage source (e.g., 230V mains)
2. Measure actual voltage with reference multimeter
3. Run console command:
   > meter cal voltage 230.2
   Starting voltage calibration (reference: 230.2V)...
   Sampling channel 0: measured 228.5V, factor 1.0074
   Sampling channel 1: measured 231.1V, factor 0.9961
   ...
   Voltage calibration complete.
   Factors saved to EEPROM.
```

**Current Calibration Procedure:**

```
1. Connect known resistive load to channel (e.g., 1000W heater = 4.35A @ 230V)
2. Measure actual current with reference clamp meter
3. Run console command:
   > meter cal current 3 4.35
   Starting current calibration for channel 3 (reference: 4.35A)...
   Sampling... measured 4.28A, factor 1.0164
   Current calibration complete.
   Factor saved to EEPROM.
```

**Calibration Storage:**

All calibration coefficients stored in EEPROM at dedicated addresses:
- 8 channels × 64 bytes = 512 bytes total
- Each entry includes voltage factor, voltage offset, current factor, current offset, resistor value, CRC-16

**Calibration Application:**

Calibration loaded automatically at boot:
```c
// In MeterTask_Init
hlw8032_load_calibration();  // Loads from EEPROM for all 8 channels
```

### 10.3 Temperature Calibration

**RP2040 Die Temperature Sensor:**

Built-in temperature sensor with typical parameters:
- **V0 @ 27°C:** 0.706V
- **Slope:** -0.001721 V/°C
- **Accuracy:** ±5°C (uncalibrated)

**Calibration Methods:**

**1. Single-Point Calibration (Offset Only):**

```
1. Stabilize system at known ambient temperature
2. Measure ambient with reference thermometer (e.g., 22.3°C)
3. Run console command:
   > temp cal single 22.3
   Current die reading: 24.8°C
   Computed offset: -2.5°C
   Calibration saved to EEPROM.
```

**2. Two-Point Calibration (Slope + Intercept):**

```
1. Stabilize system at temperature T1, measure with reference (e.g., 15.0°C)
   > temp cal point1 15.0
   Point 1 captured: T_ref=15.0°C, ADC_raw=2345

2. Heat/cool system to temperature T2, measure with reference (e.g., 40.0°C)
   > temp cal point2 40.0
   Point 2 captured: T_ref=40.0°C, ADC_raw=1892

   Computed slope: 0.001814 V/°C
   Computed V0: 0.694V
   Calibration saved to EEPROM.
```

**Calibration Query:**

```
> temp cal show
Temperature Calibration Info:
  Mode: 2-Point (Slope + Intercept)
  V0 @ 27°C: 0.694V
  Slope: 0.001814 V/°C
  Offset: 0.0°C
  Current reading: 23.1°C
```

**Calibration Validation:**

After calibration, verify accuracy:
```
> temp show
RP2040 Die Temperature: 23.1°C (calibrated)
Reference Thermometer: 23.0°C
Error: +0.1°C
```

### 10.4 Factory Reset

**Reset Options:**

1. **Soft Reset (Configuration Only):**
   - Resets network config, user prefs, channel labels
   - Preserves calibration data
   - Preserves event logs

2. **Full Reset (All Data):**
   - Resets all configuration sections
   - Clears calibration data
   - Clears event logs
   - Returns to virgin state

**Soft Reset Procedure:**

```
> factory reset config
WARNING: This will reset all configuration to factory defaults.
Calibration data and event logs will be preserved.
Type 'yes' to confirm: yes

Resetting configuration...
  - Network config: RESET
  - User preferences: RESET
  - Channel labels: RESET
  - Relay states: RESET

Configuration reset complete. Rebooting...
```

**Full Reset Procedure:**

```
> factory reset all
WARNING: This will erase ALL data including calibration!
Type 'CONFIRM' to proceed: CONFIRM

Starting full EEPROM erase...
Progress: [####################] 100%

Writing factory defaults...
Done. Rebooting...
```

**Post-Reset State:**
- Device name: "ENERGIS-PDU"
- Location: "Server Rack"
- IP: 192.168.0.22
- All relays: OFF
- All calibration: Default (uncalibrated)

---

## Appendix A: Build Instructions

**Prerequisites:**
- CMake 3.12 or higher
- ARM GCC toolchain (arm-none-eabi-gcc)
- Raspberry Pi Pico SDK

**Build Steps:**

```bash
# 1. Clone repository
git clone https://github.com/DvidMakesThings/HW_10-In-Rack_PDU.git
cd HW_10-In-Rack_PDU

# 2. Initialize Pico SDK
export PICO_SDK_PATH=/path/to/pico-sdk
cd $PICO_SDK_PATH
git submodule update --init

# 3. Configure build
cd /path/to/ENERGIS_PDU
mkdir build && cd build
cmake ..

# 4. Compile
make -j4

# 5. Flash firmware
# Option A: USB bootloader (hold BOOTSEL, plug USB, drag UF2)
cp energis_rtos.uf2 /media/RPI-RP2/

# Option B: SWD debugger (OpenOCD)
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c "program energis_rtos.elf verify reset exit"
```

**Build Targets:**

- `energis_rtos.elf` - ELF binary with debug symbols
- `energis_rtos.uf2` - UF2 bootloader format
- `energis_rtos.bin` - Raw binary
- `energis_rtos.hex` - Intel HEX format

---

## Appendix B: Debug and Diagnostics

**USB-CDC Console:**

Connect via serial terminal:
```bash
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200
```

**UART Console:**

Connect to UART1 (GPIO 8=TX, GPIO 9=RX) @ 115200 baud.

**Debug Commands:**

```
> help               - Show all commands
> version            - Show firmware version
> tasks              - Show FreeRTOS task states
> mem                - Show heap usage
> health             - Show system health
> crash              - Show last crash log
> eeprom dump        - Dump EEPROM contents
> meter show         - Show power measurements
> relay status       - Show relay states
```

**Crash Dump Format:**

```
Crash Log Entry:
  Timestamp: 123456 seconds
  Reason: WATCHDOG_TIMEOUT
  Task: NetTask
  Stack Pointer: 0x20040F80
  Link Register: 0x10001234
  Program Counter: 0x10005678
```

**EEPROM Dump Format:**

```
EE_DUMP_START
0000: 45 4E 45 52 47 49 53 5F 46 41 43 54 4F 52 59 5F
0010: 56 31 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0020: 02 45 4E C0 A8 00 16 FF FF FF 00 C0 A8 00 01 08
...
EE_DUMP_END
```

---

## Appendix C: Error Code Reference

**Format:** `0xMSCC` where M=Module, S=Severity, CC=File+Error

**Severity Levels:**
- `0x1`: INFO
- `0x2`: WARNING
- `0x4`: ERROR
- `0xF`: FATAL

**Module IDs:**
- `0x1`: InitTask
- `0x2`: NetTask
- `0x3`: MeterTask
- `0x4`: StorageTask
- `0x5`: ButtonTask
- `0x6`: HealthTask
- `0x7`: LoggerTask
- `0x8`: ConsoleTask
- `0x9`: OCP Task
- `0xA`: SwitchTask

**Common Error Codes:**

| Code | Module | Severity | Description |
|------|--------|----------|-------------|
| 0x1F00 | InitTask | FATAL | Scheduler failed to start |
| 0x2410 | NetTask | ERROR | W5500 initialization failed |
| 0x2420 | NetTask | ERROR | Ethernet link lost |
| 0x3410 | MeterTask | ERROR | HLW8032 communication timeout |
| 0x4420 | StorageTask | ERROR | EEPROM write failed |
| 0x4430 | StorageTask | ERROR | Configuration CRC invalid |
| 0x9F10 | OCP Task | FATAL | Overcurrent lockout triggered |
| 0xA410 | SwitchTask | ERROR | Relay verification failed |
| 0xA420 | SwitchTask | ERROR | I2C communication failure |

**Error Log Query:**

```
> log errors
Error Log (16 entries):
  [00:12:34] 0x2410 NetTask: W5500 init failed
  [00:15:22] 0x4420 StorageTask: EEPROM write timeout
  [01:23:45] 0x9F10 OCP: Lockout triggered (12.3A)
  ...
```

---

## Appendix D: Contact and Support

**Project Author:** DvidMakesThings - David Sipos
**GitHub Repository:** https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
**License:** (See repository LICENSE file)

**Community Support:**
- GitHub Issues: https://github.com/DvidMakesThings/HW_10-In-Rack_PDU/issues
- GitHub Discussions: https://github.com/DvidMakesThings/HW_10-In-Rack_PDU/discussions

**Hardware Documentation:**
- Schematics, PCB layouts, and BOM available in repository `hardware/` directory

**Contributing:**
- Pull requests welcome
- Please follow existing code style and documentation standards
- All submissions must pass CI/CD checks

---

**Document Version:** 1.0.0
**Last Updated:** 2026-01-06
**Generated from source code documentation**
