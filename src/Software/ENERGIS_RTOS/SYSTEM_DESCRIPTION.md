# ENERGIS PDU - Comprehensive System Description

**Document Version:** 1.0  
**Author:** DvidMakesThings - David Sipos  
**Project:** ENERGIS - The Managed PDU Project for 10-Inch Rack  
**Date:** November 2025  
**Firmware Version:** 1.1.0

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Hardware Architecture](#hardware-architecture)
3. [Software Architecture](#software-architecture)
4. [FreeRTOS Configuration](#freertos-configuration)
5. [Task Descriptions](#task-descriptions)
6. [Driver Layer](#driver-layer)
7. [Communication Protocols](#communication-protocols)
8. [Memory Architecture](#memory-architecture)
9. [Boot Sequence](#boot-sequence)
10. [Data Flow Diagrams](#data-flow-diagrams)
11. [Error Handling Strategy](#error-handling-strategy)
12. [Performance Specifications](#performance-specifications)

---

## System Overview

The ENERGIS PDU is an intelligent, rack-mountable Power Distribution Unit designed for 10-inch equipment racks with real-time power monitoring, network management, and individual outlet control capabilities.

### Key Specifications

| Category | Specification |
|----------|--------------|
| **Power** | 8 independently controlled AC outlets |
| **Monitoring** | Real-time V/I/P/E per channel at 2 Hz |
| **Network** | 10/100 Mbps Ethernet (HTTP + SNMP) |
| **Storage** | 64KB EEPROM for configuration |
| **Interface** | 4 buttons, 11 LEDs, UART console |
| **Platform** | RP2040 + FreeRTOS v11.0.0 |

---

## Hardware Architecture

### System Block Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                       ENERGIS PDU SYSTEM                            │
└─────────────────────────────────────────────────────────────────────┘
          │                     │                     │
   ┌──────┴───────┐      ┌──────┴───────┐      ┌──────┴───────┐
   │  Front Panel │      │   Network    │      │   Console    │
   │  (4 Buttons) │      │   (W5500)    │      │   (UART1)    │
   └──────┬───────┘      └──────┬───────┘      └──────┬───────┘
          │                     │                     │
          └─────────────────────┴─────────────────────┘
                                 │
                   ┌─────────────▼────────────┐
                   │       RP2040             │
                   │   Cortex-M0+ @ 125MHz    │
                   │   FreeRTOS Scheduler     │
                   └─────────────┬────────────┘
                                 │
           ┌─────────────────────┼──────────────────────┐
           │                     │                      │
    ┌──────▼───────┐      ┌──────▼───────┐       ┌──────▼───────┐
    │  I2C0 Bus    │      │  I2C1 Bus    │       │  SPI0 Bus    │
    │  (100 kHz)   │      │  (100 kHz)   │       │  (40 MHz)    │
    └──────┬───────┘      └──────┬───────┘       └──────┬───────┘
           │                     │                      │
   ┌──────┴────────┐      ┌──────┴───────┐              │
   │ MCP23017 x2   │      │ MCP23017     │              │
   │ Display 0x21  │      │ Relay 0x20   │              │
   │ Selection 0x23│      │ + CAT24C512  │              │
   │ (LED Control) │      │ (EEPROM 0x50)│              │
   └───────────────┘      └──────┬───────┘       ┌──────▼───────┐
                                 │               │   W5500      │
                          ┌──────▼───────┐       │   Ethernet   │
                          │  8 Relays    │       └──────────────┘
                          │  AC Outlets  │
                          └──────────────┘
                    
                    ┌─────────────────────────────┐
                    │   HLW8032 x8 Channels       │
                    │   UART0 @ 4800 baud         │
                    │   Power Measurement         │
                    └─────────────────────────────┘
```

### GPIO Pin Assignments

#### Communication Peripherals

| GPIO | Function | Direction | Description |
|------|----------|-----------|-------------|
| **I2C0** | | | **Display & Selection** |
| 4 | I2C0_SDA | I/O | I2C0 data line |
| 5 | I2C0_SCL | Output | I2C0 clock line |
| **I2C1** | | | **EEPROM & Relay** |
| 2 | I2C1_SDA | I/O | I2C1 data line |
| 3 | I2C1_SCL | Output | I2C1 clock line |
| **SPI0** | | | **Ethernet** |
| 16 | W5500_MISO | Input | SPI data from W5500 |
| 17 | W5500_CS | Output | SPI chip select |
| 18 | W5500_SCK | Output | SPI clock |
| 19 | W5500_MOSI | Output | SPI data to W5500 |
| **UART0** | | | **Power Meter** |
| 0 | UART0_RX | Input | HLW8032 data in |
| 1 | UART0_TX | Output | HLW8032 control out |
| **UART1** | | | **Unused** |
| 8 | UART1_TX | Output | Unused |
| 9 | UART1_RX | Input | Unused |

#### Control & Status GPIOs

| GPIO | Function | Direction | Description |
|------|----------|-----------|-------------|
| 12 | KEY_0 | Input | Minus button |
| 13 | KEY_1 | Input | Plus button |
| 14 | KEY_2 | Input | Set button |
| 15 | KEY_3 | Input | Power button |
| 20 | W5500_RESET | Output | Ethernet chip reset |
| 21 | W5500_INT | Input | Ethernet interrupt |
| 23 | MCP_MB_RST | Output | Mainboard MCP reset |
| 24 | MCP_DP_RST | Output | Display MCP reset |
| 25 | VREG_EN | Output | Voltage regulator enable |
| 26 | ADC_VUSB | Analog | USB voltage sense |
| 28 | PROC_LED | Output | Processor LED |
| 29 | ADC_12V_MEA | Analog | 12V rail measurement |

### I2C Device Map

```
┌────────────────────────────────────────────────────────────────┐
│                       I2C0 Bus @ 100 kHz                       │
├─────────┬──────────────────────┬───────────────────────────────┤
│ Address │ Device               │ Purpose                       │
├─────────┼──────────────────────┼───────────────────────────────┤
│ 0x21    │ MCP23017 Display     │ 8 outlet LEDs + 3 status LEDs │
│ 0x23    │ MCP23017 Selection   │ 8 selection indicator LEDs    │
└─────────┴──────────────────────┴───────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│                       I2C1 Bus @ 100 kHz                       │
├─────────┬──────────────────────┬───────────────────────────────┤
│ Address │ Device               │ Purpose                       │
├─────────┼──────────────────────┼───────────────────────────────┤
│ 0x20    │ MCP23017 Relay       │ 8 relays + 4-bit mux control  │
│ 0x50    │ CAT24C512 EEPROM     │ 64KB configuration storage    │
└─────────┴──────────────────────┴───────────────────────────────┘
```

### Power Measurement System

The HLW8032 power measurement system uses a hardware multiplexer to sequentially measure 8 AC channels:

```
     AC Lines (8x)              Multiplexer            HLW8032
                                                      
   ┌─────────┐                    ┌───┐           ┌──────────┐
   │Channel 1├────────────────────┤   │           │          │
   └─────────┘                    │   │           │ Voltage  │
   ┌─────────┐                    │ 8 │           │ Current  │
   │Channel 2├────────────────────┤ : ├───────────┤ Power    │
   └─────────┘                    │ 1 │           │ Energy   │
       ...                        │   │           │          │
   ┌─────────┐                    │   │           │ UART0    │
   │Channel 8├────────────────────┤   ├───────────┤ 4800baud │
   └─────────┘                    └─┬─┘           └────┬─────┘
                                    │                  │
                             MUX Control          RP2040 UART0
                          (3-bit via MCP)
```

**Measurement Specifications:**
- **Voltage:** 0-250V AC, ±1V accuracy
- **Current:** 0-20A AC, ±0.01A accuracy  
- **Power:** 0-5000W, ±5W accuracy
- **Sampling Rate:** 2 Hz per channel
- **Channel Switching:** 40ms per channel with 10ms settling

---

## Software Architecture

### FreeRTOS Task Hierarchy

```
                   Priority 23 (Highest)
                          │
                   ┌──────▼───────┐
                   │  InitTask    │  System initialization
                   └──────┬───────┘  Self-deletes after boot
                          │
        ┌─────────────────┼──────────────────┐
        │                 │                  │
    Priority 22       Priority 6        Priority 5
        │                 │                  │
   ┌────▼─────┐      ┌────▼─────┐      ┌────▼─────┐
   │ Logger   │      │ Console  │      │   Net    │
   │  Task    │      │   Task   │      │   Task   │
   └──────────┘      └──────────┘      └────┬─────┘
        │                  │                │
    Non-blocking      UART Command      W5500 Owner
    USB-CDC Queue     Processing        HTTP + SNMP
                                             │
        ┌────────────────────────────────────┼────────────┐
        │                                    │            │
    Priority 4                          Priority 2    Priority 1
        │                                    │            │
   ┌────▼─────┐                         ┌────▼─────┐ ┌───▼─────┐
   │  Meter   │                         │ Storage  │ │ Button  │
   │   Task   │                         │   Task   │ │  Task   │
   └──────────┘                         └──────────┘ └─────────┘
   HLW8032 Owner                        EEPROM Owner  Front Panel
   2 Hz Sampling                        Config Cache  Debouncing
```

### Task Priority Rationale

| Priority | Task | Rationale |
|----------|------|-----------|
| **23** | InitTask | Highest - must complete bring-up before others |
| **22** | LoggerTask | High - must drain queue to prevent USB-CDC blocks |
| **6** | ConsoleTask | Medium-high - user interaction responsiveness |
| **5** | NetTask | Medium - network packets need timely processing |
| **4** | MeterTask | Medium-low - 40ms timing is sufficient |
| **2** | StorageTask | Low - background EEPROM writes |
| **1** | ButtonTask | Lowest - 50ms debounce is not time-critical |

### Inter-Task Communication

```
┌─────────────────────────────────────────────────────────────────┐
│                    FreeRTOS Queues & Mutexes                     │
└─────────────────────────────────────────────────────────────────┘

  Queues:
  ┌─────────────────┐           ┌──────────────────┐
  │ Any Task        ├──────────►│ log_queue        │
  │ (Producers)     │  Messages │ (64 messages)    │
  └─────────────────┘           └────────┬─────────┘
                                         │
                                         ▼
                                ┌────────────────┐
                                │  LoggerTask    │
                                │  (Consumer)    │
                                └────────────────┘

  ┌─────────────────┐           ┌──────────────────┐
  │  MeterTask      ├──────────►│ q_meter_telemetry│
  │  (Producer)     │  Telemetry│ (8 channels)     │
  └─────────────────┘           └────────┬─────────┘
                                         │
                                         ▼
                                ┌────────────────┐
                                │  NetTask       │
                                │  StorageTask   │
                                │  (Consumers)   │
                                └────────────────┘

  ┌─────────────────┐           ┌──────────────────┐
  │  NetTask        ├──────────►│ q_cfg            │
  │  ButtonTask     │  Config   │ (Configuration)  │
  │  (Producers)    │  Requests │                  │
  └─────────────────┘           └────────┬─────────┘
                                         │
                                         ▼
                                ┌────────────────┐
                                │  StorageTask   │
                                │  (Consumer)    │
                                └────────────────┘

  Mutexes (Resource Protection):
  ┌──────────────┐
  │  i2cMtx      │  Protects I2C0 bus (Display & Selection MCPs)
  └──────────────┘
  ┌──────────────┐
  │  eepromMtx   │  Protects I2C1 bus (EEPROM & Relay MCP)
  └──────────────┘
  ┌──────────────┐
  │  spiMtx      │  Protects SPI0 bus (W5500 Ethernet)
  └──────────────┘

  Event Groups:
  ┌──────────────┐
  │  cfgEvents   │  Configuration ready flags
  │              │  - CFG_READY: Storage initialized
  │              │  - NET_READY: Network configured
  └──────────────┘
```

### Peripheral Ownership Model

**CRITICAL DESIGN PRINCIPLE:** Each peripheral is owned by exactly ONE task to prevent race conditions and ensure thread safety.

| Peripheral | Owner Task | Access Method |
|------------|-----------|---------------|
| **HLW8032 (UART0)** | MeterTask | Exclusive - no mutex needed |
| **W5500 (SPI0)** | NetTask | Exclusive with spiMtx for sharing |
| **Console (USB-CDC)** | ConsoleTask | Exclusive - no mutex needed |
| **USB-CDC** | LoggerTask | Exclusive - queue-based access |
| **EEPROM (I2C1)** | StorageTask | Exclusive with eepromMtx |
| **Relay MCP (I2C1)** | StorageTask | Via eepromMtx (same bus) |
| **Display MCPs (I2C0)** | ButtonTask | Via i2cMtx (shared bus) |
| **Front Panel Buttons** | ButtonTask | Exclusive - no mutex needed |

---

## FreeRTOS Configuration

### Key Configuration Parameters

```c
// From FreeRTOSConfig.h

/* Scheduler */
#define configCPU_CLOCK_HZ              125000000  // 125 MHz
#define configTICK_RATE_HZ              1000       // 1ms tick
#define configMAX_PRIORITIES            24         // Priority levels
#define configMINIMAL_STACK_SIZE        512        // Words
#define configUSE_PREEMPTION            1          // Enabled
#define configUSE_TIME_SLICING          1          // Enabled

/* Memory */
#define configTOTAL_HEAP_SIZE           (128*1024) // 128 KB heap
#define configSUPPORT_DYNAMIC_ALLOCATION 1         // Dynamic only

/* Features */
#define configUSE_MUTEXES               1          // Essential
#define configUSE_TASK_NOTIFICATIONS    1          // Efficient signaling
#define configUSE_TIMERS                1          // Software timers
#define configTIMER_TASK_PRIORITY       23         // Highest

/* Safety */
#define configCHECK_FOR_STACK_OVERFLOW  2          // Method 2 (thorough)
#define configUSE_MALLOC_FAILED_HOOK    1          // Catch OOM
```

### Memory Budget

```
┌────────────────────────────────────────────────────────────────┐
│                   RP2040 Memory Usage (264 KB)                 │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  FreeRTOS Kernel Data:          ~8 KB                          │
│  ├─ Task control blocks                                        │
│  ├─ Queue structures                                           │
│  ├─ Mutex structures                                           │
│  └─ Event groups                                               │
│                                                                │
│  Task Stacks:                   ~48 KB                         │
│  ├─ InitTask:       2048 words  →  8 KB                        │
│  ├─ LoggerTask:     1024 words  →  4 KB                        │
│  ├─ ConsoleTask:    2048 words  →  8 KB                        │
│  ├─ NetTask:        2048 words  →  8 KB                        │
│  ├─ MeterTask:      2048 words  →  8 KB                        │
│  ├─ StorageTask:    2048 words  →  8 KB                        │
│  └─ ButtonTask:     1024 words  →  4 KB                        │
│                                                                │
│  FreeRTOS Heap:                 128 KB                         │
│  ├─ Dynamic allocations                                        │
│  ├─ Queue storage                                              │
│  └─ Buffer allocations                                         │
│                                                                │
│  Application Data:              ~32 KB                         │
│  ├─ Driver state structures                                    │
│  ├─ Network buffers                                            │
│  ├─ Telemetry cache                                            │
│  └─ Configuration cache                                        │
│                                                                │
│  Available:                     ~80 KB                         │
│                                                                │
│  TOTAL:                         264 KB                         │
└────────────────────────────────────────────────────────────────┘
```

---

## Task Descriptions

### 1. InitTask (Priority 23)

**Purpose:** Deterministic system bring-up sequencing

**Lifecycle:** Created by main(), runs once, self-deletes after completion

**Initialization Sequence:**
```
Phase 0: Logger Initialization
  └─ Start LoggerTask FIRST (prevent USB-CDC stalls)
  └─ Wait for Logger ready signal (2s timeout)

Phase 1: Hardware Initialization
  └─ GPIO configuration (LEDs, resets, chip selects)
  └─ I2C0 initialization (Display & Selection MCPs)
  └─ I2C1 initialization (EEPROM & Relay MCP)
  └─ SPI0 initialization (W5500 Ethernet)
  └─ ADC initialization (voltage/temperature sensing)
  └─ Peripheral resets (W5500, MCPs)

Phase 2: Peripheral Probing
  └─ Probe MCP23017 devices (0x20, 0x21, 0x23)
  └─ Probe EEPROM (0x50)
  └─ Log detection results

Phase 3: Task Creation (in dependency order)
  └─ ConsoleTask  (independent, starts immediately)
  └─ StorageTask  (EEPROM owner, loads configuration)
  └─ ButtonTask   (front panel, depends on MCP init)
  └─ NetTask      (waits for config from StorageTask)
  └─ MeterTask    (power metering, independent)
  
Phase 4: Configuration Distribution
  └─ Wait for StorageTask config ready (10s timeout)
  └─ Retrieve network configuration
  └─ Log loaded configuration

Phase 5: Finalization
  └─ Set Power Good LED (green)
  └─ Clear Error LED (red)
  └─ Log "System bring-alive complete"
  └─ Self-delete
```

**Key Functions:**
- `InitTask_Create()` - Creates the task
- `InitTask_Run()` - Main initialization logic

**Stack Size:** 2048 words (8 KB)

**Timeout Handling:** Each task creation waits up to 5 seconds for ready signal. Timeouts are logged but system continues.

---

### 2. LoggerTask (Priority 22)

**Purpose:** Non-blocking logging system to prevent USB-CDC stalls

**Problem Solved:** Old system used blocking `printf()` which could freeze the entire system for seconds when USB was not ready.

**Architecture:**
```
  Any Task                    Queue                   LoggerTask
     │                          │                          │
     │ log_printf("msg")        │                          │
     ├─────────────────────────►│                          │
     │  (non-blocking!)         │                          │
     │  Returns immediately     │                          │
     │                          │                          │
     │                          │  xQueueReceive()         │
     │                          ◄──────────────────────────┤
     │                          │                          │
     │                          │  Process message         │
     │                          │  ├─ USB-CDC (if ready)   │
     │                          │  └─ UART1 (always)       │
     │                          │                          │
```

**Key Functions:**
- `log_printf()` - Non-blocking log message submission
- `Logger_IsReady()` - Check if logger is operational

**Configuration:**
- **Queue Size:** 64 messages
- **Message Length:** 128 bytes max
- **Stack Size:** 1024 words (4 KB)

**Output Destinations:**
1. **USB-CDC:** Exclusive output (owns stdio)
Note: UART1 is unused by this task

**Message Format:**
```
[LEVEL] Message text
```
Where LEVEL = INFO, DEBUG, ERROR, WARNING, NETLOG, PLOT, ECHO

---

### 3. ConsoleTask (Priority 6)

**Purpose:** Interactive USB-CDC command-line interface

**Interface Configuration:**
- **Port:** USB-CDC (reads input only)
- **Output:** All prints sent through Logger macros
Note: UART1 is unused by this task

**Command Processing:**
```
User Input (UART1) → Line Buffer (1024 bytes)
                           ↓
                    Command Parser
                           ↓
            ┌──────────────┼──────────────┐
            │              │              │
    ┌───────▼──────┐ ┌─────▼─────┐ ┌────▼──────┐
    │ System Cmds  │ │ Net Cmds  │ │ Cfg Cmds  │
    │ - status     │ │ - netinfo │ │ - save    │
    │ - uptime     │ │ - ping    │ │ - load    │
    │ - tasks      │ │ - socket  │ │ - default │
    └──────────────┘ └───────────┘ └───────────┘
```

**Available Commands:**
- `help` - Display command list
- `status` - System status and telemetry
- `netinfo` - Network configuration
- `outlet <n> <on|off>` - Control outlet
- `reboot` - Software reset
- `bootsel` - Enter bootloader mode

**Stack Size:** 2048 words (8 KB)

---

### 4. StorageTask (Priority 2)

**Purpose:** Exclusive EEPROM owner and configuration manager

**Key Responsibilities:**
1. Own ALL EEPROM access (thread-safe)
2. Maintain RAM cache of configuration
3. Debounce writes (2-second idle period)
4. Process configuration change requests via queue

**Configuration Cache Structure:**
```c
typedef struct {
    networkInfo network;        // IP, subnet, gateway, MAC, DNS
    uint8_t relay_status[8];    // Relay on/off states
    userPrefInfo user_prefs;    // Device name, location, temp unit
    float energy_data[8];       // Energy accumulation per channel
    uint32_t uptime;            // System runtime
    bool dirty_flags[8];        // Track what needs writing
} storage_cache_t;
```

**EEPROM Memory Map:**
```
Address Range    Size    Purpose
────────────────────────────────────────────────────────
0x0000-0x001F    32B    System Info Block
                        - Magic: 0xE5 (validation)
                        - Version
                        - Serial number
                        - Checksum

0x0020-0x003F    32B    Network Configuration
                        - IP address (4 bytes)
                        - Subnet mask (4 bytes)
                        - Gateway (4 bytes)
                        - MAC address (6 bytes)
                        - DNS server (4 bytes)
                        - DHCP enable (1 byte)
                        - Reserved (9 bytes)

0x0040-0x004F    16B    Relay Status
                        - Outlet states (8 bytes)
                        - Reserved (8 bytes)

0x0050-0x00AF    96B    User Preferences
                        - Device name (32 bytes)
                        - Location (32 bytes)
                        - Temperature unit (1 byte)
                        - Reserved (31 bytes)

0x00B0-0x00EF    64B    Energy Data
                        - Channel 0-7 energy (8x4 = 32 bytes)
                        - Timestamps (8x4 = 32 bytes)

0x00F0-0x01EF   256B    Event Log
                        - 16 events x 16 bytes each

0x01F0-0x7FFF   ~32KB   Reserved / User Data
```

**Write Debouncing:**
- Configuration changes are queued
- 2-second idle period must pass before writing
- Prevents excessive wear on EEPROM
- Coalesces multiple rapid changes into single write

**Key Functions:**
- `storage_get_network()` - Read network configuration
- `storage_set_network()` - Update network configuration
- `storage_get_relay()` - Read relay state
- `storage_set_relay()` - Update relay state
- `Storage_IsReady()` - Check if initialized

**Stack Size:** 2048 words (8 KB)

---

### 5. NetTask (Priority 5)

**Purpose:** W5500 Ethernet controller owner and network service manager

**Services Provided:**
1. **HTTP Server** (Socket 0)
2. **SNMP Agent** (Socket 1)
3. **SNMP Trap** (Socket 2)

**Initialization Sequence:**
```
1. Wait for StorageTask config ready (blocks until available)
2. Retrieve network configuration from storage
3. Initialize W5500 hardware
   ├─ Hardware reset sequence
   ├─ SPI communication test
   └─ Read version register (expect 0x04)
4. Apply network configuration
   ├─ Set MAC address
   ├─ Configure IP/subnet/gateway
   ├─ Apply DHCP or static mode
   └─ Enable PHY
5. Wait for link up (30s timeout)
6. Start service loops
   ├─ HTTP server on port 80
   └─ SNMP agent on port 161
```

**HTTP Server Architecture:**
```
TCP Socket 0 (Port 80)
     │
     ▼
┌─────────────────────────────────┐
│   HTTP Request Parser           │
│   - Method (GET/POST)           │
│   - URI extraction              │
│   - Content-Length parsing      │
└─────────┬───────────────────────┘
          │
    ┌─────┴──────┬──────────┬──────────────┐
    │            │          │              │
┌───▼────┐  ┌───▼────┐ ┌───▼──────┐  ┌───▼──────┐
│ Static │  │Control │ │ Settings │  │  Status  │
│ Pages  │  │Handler │ │ Handler  │  │ Handler  │
└────────┘  └────────┘ └──────────┘  └──────────┘
    │            │          │              │
    │       ┌────┴──────────┴──────────────┘
    │       │
    └───────┴─────► HTTP Response Generator
                        │
                        ▼
                   Send to Client
```

**Supported URIs:**
- `/` - Main control page
- `/control` - Outlet control API
- `/settings` - Configuration API
- `/status` - Telemetry API  
- `/help` - Documentation

**SNMP Implementation:**
- **Version:** SNMPv1, SNMPv2c
- **Community Strings:** Configurable
- **MIBs Supported:**
  - **System Group:** sysDescr, sysUpTime, sysContact, sysName
  - **Interface Group:** ifNumber, ifDescr, ifType, ifOperStatus
  - **Custom PDU MIB:** Outlet control, power telemetry

**Key Functions:**
- `NetTask_Init()` - Create task
- `Net_IsReady()` - Check if network is operational
- `http_server_process()` - HTTP request handler
- `snmp_process()` - SNMP packet handler

**Stack Size:** 2048 words (8 KB)

---

### 6. MeterTask (Priority 4)

**Purpose:** HLW8032 power meter owner and telemetry publisher

**Measurement Strategy:**

```
Round-Robin Channel Polling (2 Hz per channel):

Time (ms)    Channel    Action
─────────────────────────────────────────
0            0          Switch mux to CH0
10           0          Read HLW8032 (50ms)
60           1          Switch mux to CH1
70           1          Read HLW8032 (50ms)
120          2          Switch mux to CH2
130          2          Read HLW8032 (50ms)
...
300          7          Switch mux to CH7
310          7          Read HLW8032 (50ms)
360          0          Cycle repeats
```

**Per-Channel Processing:**
```
Raw HLW8032 Data (24-byte frame)
      │
      ▼
┌─────────────────────┐
│  Parse Frame        │
│  - Voltage bytes    │
│  - Current bytes    │
│  - Power bytes      │
│  - Validate CRC     │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│  Apply Calibration  │
│  - V = raw * V_cal  │
│  - I = raw * I_cal  │
│  - P = raw * P_cal  │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│  Update Averages    │
│  - 1-second window  │
│  - 25 samples/sec   │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│  Accumulate Energy  │
│  - E += P * Δt      │
│  - Store in kWh     │
└──────┬──────────────┘
       │
       ▼
┌─────────────────────┐
│  Publish Telemetry  │
│  - Queue message    │
│  - Format for SNMP  │
│  - Update cache     │
└─────────────────────┘
```

**Telemetry Data Structure:**
```c
typedef struct {
    uint8_t channel;        // 0-7
    float voltage;          // RMS voltage (V)
    float current;          // RMS current (A)
    float power;            // Active power (W)
    float energy;           // Accumulated energy (kWh)
    float power_factor;     // 0.0-1.0
    uint32_t timestamp_ms;  // Measurement time
    bool valid;             // Data validity flag
} meter_telemetry_t;
```

**Calibration Storage:**
- Per-channel calibration coefficients stored in EEPROM
- Factory defaults if not calibrated
- Accessible via console commands

**Key Functions:**
- `MeterTask_Init()` - Create task
- `Meter_IsReady()` - Check if operational
- `meter_get_telemetry()` - Retrieve latest data

**Stack Size:** 2048 words (8 KB)

---

### 7. ButtonTask (Priority 1)

**Purpose:** Front panel button scanning and LED control

**Button Configuration:**

| Button | GPIO | Function |
|--------|------|----------|
| KEY_0 | 12 | Minus / Decrease |
| KEY_1 | 13 | Plus / Increase |
| KEY_2 | 14 | Set / Enter |
| KEY_3 | 15 | Power / Menu |

**Debouncing Strategy:**
```
Button State Machine (per button):

         IDLE
           │
           │ Press detected
           ▼
       DEBOUNCE
      (100ms wait)
           │
           │ Still pressed
           ▼
       PRESSED
           │
           │ Release detected
           ▼
       DEBOUNCE
      (100ms wait)
           │
           │ Still released
           ▼
         IDLE
```

**LED Control:**
- **3 Status LEDs** (via MCP23017 Display, 0x21)
  - Power Good (Green) - GPB2
  - Network Link (Blue) - GPB1
  - Fault (Red) - GPB0

- **8 Outlet LEDs** (via MCP23017 Display, 0x21)
  - OUT_1 through OUT_8 (GPIO 0-7)
  - Indicate relay states

- **8 Selection LEDs** (via MCP23017 Selection, 0x23)
  - SEL_1 through SEL_8 (GPIO 0-7)
  - Menu navigation indicators

**Key Functions:**
- `ButtonTask_Init()` - Create task
- `Button_IsReady()` - Check if operational
- `button_get_state()` - Read button state
- `setPowerGood()`, `setNetworkLink()`, `setError()` - LED control

**Stack Size:** 1024 words (4 KB)

---

## Driver Layer

### 1. CAT24C512 EEPROM Driver

**File:** `CAT24C512_driver.c/h`

**Specifications:**
- **Capacity:** 64KB (512 Kbit)
- **Interface:** I2C (100 kHz)
- **Address:** 0x50
- **Page Size:** 128 bytes
- **Write Time:** 5ms per page

**Key Features:**
- Page-aligned writes for efficiency
- Write-in-progress polling
- CRC-8 checksums for data integrity
- Thread-safe via eepromMtx

**API:**
```c
void CAT24C512_Init(void);
bool CAT24C512_Write(uint16_t addr, const uint8_t* data, size_t len);
bool CAT24C512_Read(uint16_t addr, uint8_t* data, size_t len);
bool CAT24C512_WritePage(uint16_t addr, const uint8_t* data);
```

---

### 2. MCP23017 GPIO Expander Driver

**File:** `MCP23017_driver.c/h`

**Three MCP23017 Devices:**
1. **Relay Board (0x20)** on I2C1
   - 8 relay outputs (GPIOA 0-7)
   - 4 mux control bits (GPIOA 8-11)

2. **Display Board (0x21)** on I2C0
   - 8 outlet LEDs (GPIOA 0-7)
   - 3 status LEDs (GPIOB 0-2)

3. **Selection Board (0x23)** on I2C0
   - 8 selection LEDs (GPIOA 0-7)

**Key Features:**
- 16-bit I/O expander (2 x 8-bit ports)
- Individual pin configuration
- Internal pull-ups available
- Interrupt capability (not used)

**API:**
```c
void MCP23017_Init(void);
void mcp_set_pin(i2c_inst_t* i2c, uint8_t addr, uint8_t pin, bool state);
bool mcp_get_pin(i2c_inst_t* i2c, uint8_t addr, uint8_t pin);
void mcp_set_port(i2c_inst_t* i2c, uint8_t addr, uint8_t port, uint8_t value);
```

---

### 3. HLW8032 Power Meter Driver

**File:** `HLW8032_driver.c/h`

**Communication:**
- **Interface:** UART0 @ 4800 baud
- **Frame Format:** 24 bytes binary
- **Checksum:** Sum of all bytes

**Frame Structure:**
```
Byte  | Description
──────┼────────────────────────────
0     | Header (0x55)
1-2   | Voltage parameter (16-bit)
3-4   | Voltage (16-bit ADC value)
5-6   | Current parameter (16-bit)
7-8   | Current (16-bit ADC value)
9-10  | Power parameter (16-bit)
11-12 | Power (16-bit ADC value)
13-14 | Data update flag
15-16 | PF pulse count
17-20 | Reserved
21-22 | Checksum
23    | Tail (0xAA)
```

**Calibration:**
- Voltage coefficient (V_coef)
- Current coefficient (I_coef)
- Power coefficient (P_coef)
- Stored per channel in EEPROM

**API:**
```c
void HLW8032_Init(void);
bool HLW8032_ReadFrame(uint8_t* frame);
bool HLW8032_ParseData(const uint8_t* frame, hlw8032_data_t* data);
void HLW8032_SelectChannel(uint8_t channel);
```

---

### 4. W5500 Ethernet Driver

**Files:** `ethernet_driver.c/h`, `socket.c/h`, `ethernet_w5500regs.h`

**Architecture:**
- **Complete independence from WIZnet libraries**
- Custom socket API implementation
- Hardware TCP/IP stack utilization

**Socket API:**
```c
int8_t socket_open(uint8_t sn, uint8_t protocol, uint16_t port);
int8_t socket_close(uint8_t sn);
int8_t socket_listen(uint8_t sn);
int32_t socket_send(uint8_t sn, const uint8_t* buf, uint16_t len);
int32_t socket_recv(uint8_t sn, uint8_t* buf, uint16_t len);
uint8_t socket_get_status(uint8_t sn);
```

**Network Configuration:**
```c
typedef struct {
    uint8_t mac[6];      // MAC address
    uint8_t ip[4];       // IP address
    uint8_t subnet[4];   // Subnet mask
    uint8_t gateway[4];  // Gateway
    uint8_t dns[4];      // DNS server
    bool dhcp_enable;    // DHCP/Static
} w5500_NetConfig;
```

**Features:**
- Hardware TCP/IP processing
- 8 independent sockets
- Auto-negotiation (10/100 Mbps)
- Link status monitoring
- SPI @ 40 MHz

**API:**
```c
bool w5500_hw_init(void);
bool w5500_chip_init(void);
void w5500_set_network_config(const w5500_NetConfig* cfg);
uint8_t w5500_get_link_status(void);
```

---

## Communication Protocols

### HTTP Protocol Implementation

**Supported Methods:**
- **GET:** Retrieve pages and status
- **POST:** Submit configuration changes

**Request Flow:**
```
Client TCP Connect
     │
     ▼
┌──────────────────────────────┐
│ Read HTTP Request            │
│ - Parse method               │
│ - Extract URI                │
│ - Parse headers              │
└──────────┬───────────────────┘
           │
           ▼
┌─────────────────────────────────┐
│ Route to Handler                │
│ ├─ /        → index.html        │
│ ├─ /control → control_handler   │
│ ├─ /settings → settings_handler │
│ └─ /status  → status_handler    │
└──────────┬──────────────────────┘
           │
           ▼
┌──────────────────────────────┐
│ Generate Response            │
│ - HTTP headers               │
│ - Content-Type               │
│ - Content-Length             │
│ - Response body              │
└──────────┬───────────────────┘
           │
           ▼
   Send to Client & Close
```

**Response Format:**
```http
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 1234
Connection: close

<!DOCTYPE html>
<html>...
```

---

### SNMP Protocol Implementation

**Supported Versions:**
- SNMPv1
- SNMPv2c

**SNMP PDU Types:**
- **GetRequest** - Read single OID
- **GetNextRequest** - Walk MIB tree
- **SetRequest** - Write OID value
- **GetResponse** - Response to requests
- **Trap** - Unsolicited notifications

**Custom PDU MIB Structure:**
```
.1.3.6.1.4.1.19865 (Enterprise OID)
    └─ .1 (PDU System)
        ├─ .1 (Identification)
        │   ├─ .1 - Device Name
        │   ├─ .2 - Serial Number
        │   ├─ .3 - Firmware Version
        │   └─ .4 - Uptime
        │
        ├─ .2 (Network Configuration)
        │   ├─ .1 - IP Address
        │   ├─ .2 - Subnet Mask
        │   ├─ .3 - Gateway
        │   └─ .4 - MAC Address
        │
        ├─ .3 (Outlet Control)
        │   ├─ .1 - Outlet 1 State (RW)
        │   ├─ .2 - Outlet 2 State (RW)
        │   ├─ ...
        │   └─ .8 - Outlet 8 State (RW)
        │
        └─ .4 (Power Monitoring)
            ├─ .1 (Channel 1)
            │   ├─ .1 - Voltage
            │   ├─ .2 - Current
            │   ├─ .3 - Power
            │   └─ .4 - Energy
            ├─ .2 (Channel 2)
            └─ ...
```

**SNMP Trap Conditions:**
- Outlet state change
- Power threshold exceeded
- Link status change
- Configuration change

---

## Memory Architecture

### Flash Memory Layout (16 MB)

```
0x10000000  ┌──────────────────────────┐
            │  Bootloader (RP2040)     │  256 KB
0x10040000  ├──────────────────────────┤
            │  Firmware Code           │  ~210 KB
            │  - FreeRTOS kernel       │
            │  - Application code      │
            │  - Drivers               │
            │  - Protocol stacks       │
0x10070000  ├──────────────────────────┤
            │  Static Web Content      │  ~50 KB
            │  - HTML pages            │
            │  - CSS/JS embedded       │
0x10080000  ├──────────────────────────┤
            │  Unused / Reserved       │  ~1.5 MB
            │  - Future expansion      │
0x10200000  └──────────────────────────┘
```

### RAM Memory Layout (264 KB)

```
0x20000000  ┌──────────────────────────┐
            │  FreeRTOS Kernel Data    │  ~8 KB
            │  - TCBs, queues, mutexes │
0x20002000  ├──────────────────────────┤
            │  Task Stacks             │  ~48 KB
            │  - InitTask: 8KB         │
            │  - LoggerTask: 4KB       │
            │  - ConsoleTask: 8KB      │
            │  - NetTask: 8KB          │
            │  - MeterTask: 8KB        │
            │  - StorageTask: 8KB      │
            │  - ButtonTask: 4KB       │
0x2000E000  ├──────────────────────────┤
            │  FreeRTOS Heap           │  128 KB
            │  - Dynamic allocations   │
            │  - Network buffers       │
0x2002E000  ├──────────────────────────┤
            │  Application Data        │  ~32 KB
            │  - Driver structures     │
            │  - Config cache          │
            │  - Telemetry buffers     │
0x20036000  ├──────────────────────────┤
            │  Stack (Core 0)          │  ~32 KB
0x2003E000  ├──────────────────────────┤
            │  Stack (Core 1-unused)   │  ~16 KB
0x20042000  └──────────────────────────┘
```

---

## Boot Sequence

### Detailed Boot Flow

```
Power-On
   │
   ▼
┌────────────────────────────────────────┐
│  RP2040 Bootloader (ROM)               │
│  - Initialize clock to 125 MHz         │
│  - Initialize USB (if BOOTSEL pressed) │
│  - Jump to flash @ 0x10000000          │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  main() - Bare Metal Context           │
│  ├─ stdio_init_all()                   │
│  │   (USB-CDC enumeration)             │
│  ├─ Sleep 1000ms                       │
│  ├─ Print boot banner                  │
│  ├─ InitTask_Create()                  │
│  │   (Highest priority)                │
│  └─ vTaskStartScheduler()              │
│      (NEVER RETURNS)                   │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  FreeRTOS Scheduler Active             │
│  - SysTick timer @ 1kHz                │
│  - Task switching enabled              │
│  - InitTask starts immediately         │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  InitTask - Phase 0                    │
│  ├─ Create LoggerTask (priority 22)    │
│  └─ Wait for logger ready (2s)         │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  InitTask - Phase 1                    │
│  ├─ GPIO configuration                 │
│  ├─ I2C0 init (100 kHz)                │
│  ├─ I2C1 init (100 kHz)                │
│  ├─ SPI0 init (40 MHz)                 │
│  ├─ ADC init                           │
│  ├─ W5500 reset sequence               │
│  ├─ MCP23017 initialization            │
│  └─ CAT24C512 initialization           │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  InitTask - Phase 2                    │
│  ├─ Probe MCP @ 0x20 (Relay)           │
│  ├─ Probe MCP @ 0x21 (Display)         │
│  ├─ Probe MCP @ 0x23 (Selection)       │
│  └─ Probe EEPROM @ 0x50                │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  InitTask - Phase 3                    │
│  ├─ Create ConsoleTask (priority 6)    │
│  │   Wait for ready (5s timeout)       │
│  ├─ Create StorageTask (priority 2)    │
│  │   Wait for ready (5s timeout)       │
│  ├─ Create ButtonTask (priority 1)     │
│  │   Wait for ready (5s timeout)       │
│  ├─ Create NetTask (priority 5)        │
│  │   Wait for ready (5s timeout)       │
│  └─ Create MeterTask (priority 4)      │
│      Wait for ready (5s timeout)       │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  InitTask - Phase 4                    │
│  ├─ Wait for config ready (10s)        │
│  ├─ Load network configuration         │
│  └─ Log configuration to console       │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  InitTask - Phase 5                    │
│  ├─ Set Power Good LED (green)         │
│  ├─ Clear Error LED (red)              │
│  ├─ Log "System ready"                 │
│  └─ vTaskDelete(NULL) - Self-delete    │
└─────────────────┬──────────────────────┘
                  │
                  ▼
┌────────────────────────────────────────┐
│  Normal Operation                      │
│  All subsystem tasks running           │
│  - LoggerTask: Draining queue          │
│  - ConsoleTask: Processing commands    │
│  - NetTask: Serving HTTP & SNMP        │
│  - MeterTask: Measuring power @ 2Hz   │
│  - StorageTask: Managing configuration │
│  - ButtonTask: Scanning buttons        │
└────────────────────────────────────────┘
```

**Boot Time Budget:**
- **Total:** ~5-8 seconds (worst case)
- **Phase 0 (Logger):** 2s
- **Phase 1 (Hardware):** 0.5s
- **Phase 2 (Probing):** 0.5s
- **Phase 3 (Tasks):** 1-5s (depends on EEPROM, network)
- **Phase 4 (Config):** 0.1s
- **Phase 5 (Finalize):** 0.1s

---

## Data Flow Diagrams

### Configuration Management Flow

```
┌──────────────┐
│   User       │  (Web, Console, SNMP)
└──────┬───────┘
       │ Change request
       ▼
┌───────────────────────────┐
│  NetTask / ConsoleTask    │
│  - Parse request          │
│  - Validate parameters    │
└──────┬────────────────────┘
       │ Queue message
       ▼
┌───────────────────────────┐
│  q_cfg Queue              │
│  (Configuration requests) │
└──────┬────────────────────┘
       │
       ▼
┌───────────────────────────┐
│  StorageTask              │
│  ├─ Update RAM cache      │
│  ├─ Set dirty flag        │
│  ├─ Start debounce timer  │
│  └─ (2s idle period)      │
└──────┬────────────────────┘
       │ After 2s idle
       ▼
┌───────────────────────────┐
│  EEPROM Write             │
│  ├─ Take eepromMtx        │
│  ├─ Write pages           │
│  ├─ Update checksum       │
│  └─ Release eepromMtx     │
└──────┬────────────────────┘
       │ Propagate change
       ▼
┌───────────────────────────┐
│  Affected Subsystems      │
│  ├─ NetTask (if network)  │
│  ├─ MeterTask (if calibr.)│
│  └─ ButtonTask (if LEDs)  │
└───────────────────────────┘
```

### Power Telemetry Flow

```
┌───────────────────────────┐
│  MeterTask (40ms loop)    │
│  ├─ Select channel        │
│  ├─ Wait 10ms settling    │
│  ├─ Read HLW8032 frame    │
│  ├─ Validate checksum     │
│  ├─ Apply calibration     │
│  ├─ Update averages       │
│  └─ Accumulate energy     │
└──────┬────────────────────┘
       │ Publish telemetry
       ▼
┌───────────────────────────┐
│  q_meter_telemetry Queue  │
│  (8 channel buffers)      │
└──────┬────────────────────┘
       │
       ├─────────────────┬─────────────────┐
       │                 │                 │
       ▼                 ▼                 ▼
┌──────────────┐  ┌─────────────┐  ┌──────────────┐
│  NetTask     │  │ StorageTask │  │ ConsoleTask  │
│  - HTTP API  │  │ - Energy    │  │ - Status cmd │
│  - SNMP OIDs │  │   persist   │  │ - Live data  │
└──────────────┘  └─────────────┘  └──────────────┘
```

### Network Request Handling Flow

```
Ethernet Packet Arrival
       │
       ▼
┌───────────────────────────┐
│  W5500 Hardware           │
│  ├─ Frame reception       │
│  ├─ TCP/IP processing     │
│  └─ Store in socket buffer│
└──────┬────────────────────┘
       │ Interrupt (GPIO21)
       ▼
┌───────────────────────────┐
│  NetTask                  │
│  ├─ Poll socket status    │
│  ├─ Read from socket      │
│  └─ Parse protocol        │
└──────┬────────────────────┘
       │
   ┌───┴────┐
   │        │
   ▼        ▼
┌─────┐  ┌──────┐
│HTTP │  │ SNMP │
└──┬──┘  └───┬──┘
   │         │
   ▼         ▼
┌─────────────────────────┐
│  Request Handlers       │
│  ├─ GET /status         │
│  ├─ POST /control       │
│  ├─ SNMP GetRequest     │
│  └─ SNMP SetRequest     │
└──────┬──────────────────┘
       │ Access subsystems
       ▼
┌───────────────────────────┐
│  Data Sources             │
│  ├─ StorageTask (config)  │
│  ├─ MeterTask (telemetry) │
│  └─ ButtonTask (status)   │
└──────┬────────────────────┘
       │
       ▼
┌───────────────────────────┐
│  Generate Response        │
│  ├─ Format data           │
│  ├─ Create headers        │
│  └─ Send via socket       │
└───────────────────────────┘
```

---

## Error Handling Strategy

### Task-Level Error Handling

Each task implements graceful degradation:

| Task | Critical Failure | Response |
|------|------------------|----------|
| **InitTask** | Peripheral not detected | Log warning, continue boot |
| **LoggerTask** | Queue full | Drop oldest message |
| **ConsoleTask** | UART error | Reset UART, continue |
| **NetTask** | W5500 comm fail | Retry init, operate degraded |
| **MeterTask** | HLW8032 timeout | Mark channel invalid, continue |
| **StorageTask** | EEPROM read fail | Use defaults, mark dirty |
| **ButtonTask** | MCP comm fail | Retry, log error |

### System-Level Failure Responses

```
┌─────────────────────────────────────────────────────────────┐
│                    Error Condition                          │
├─────────────────────────────────────────────────────────────┤
│  Heap Exhausted (malloc failure)                            │
│  └─ vApplicationMallocFailedHook()                          │
│      ├─ Log error                                           │
│      ├─ Set Error LED                                       │
│      └─ Halt in infinite loop (safety)                      │
│                                                             │
│  Stack Overflow (task)                                      │
│  └─ vApplicationStackOverflowHook()                         │
│      ├─ Log task name                                       │
│      ├─ Set Error LED                                       │
│      └─ Halt in infinite loop (prevent corruption)          │
│                                                             │
│  Watchdog Timeout (if enabled)                              │
│  └─ System reset                                            │
│      ├─ Boot into safe mode                                 │
│      └─ Log reset reason to EEPROM                          │
│                                                             │
│  Peripheral Communication Failure                           │
│  └─ Task-specific recovery                                  │
│      ├─ Retry with exponential backoff                      │
│      ├─ Log error to console                                │
│      └─ Continue with reduced functionality                 │
└─────────────────────────────────────────────────────────────┘
```

### Logging Levels for Diagnostics

| Level | Purpose | Destination |
|-------|---------|-------------|
| **DEBUG** | Development tracing | USB-CDC, UART1 |
| **INFO** | Normal operation | USB-CDC, UART1 |
| **WARNING** | Non-critical issues | USB-CDC, UART1, Network |
| **ERROR** | Failure conditions | USB-CDC, UART1, Network |

---

## Performance Specifications

### Timing Guarantees (with FreeRTOS)

| Operation | Latency | Jitter |
|-----------|---------|--------|
| **Button Response** | 10-50ms | ±10ms |
| **Power Measurement** | 40ms per channel | ±1ms |
| **HTTP Request** | 50-150ms | ±20ms |
| **SNMP Query** | 10-50ms | ±5ms |
| **EEPROM Write** | 5ms per page | None |
| **Log Message** | <1ms | None |
| **Configuration Save** | 320ms (background) | None |

### Throughput Specifications

| Metric | Specification |
|--------|--------------|
| **HTTP Requests/sec** | 20 (concurrent processing) |
| **SNMP Queries/sec** | 50 (lightweight protocol) |
| **Power Measurements/sec** | 25 per channel (200 total/sec) |
| **Ethernet Throughput** | ~5 Mbps (limited by application) |
| **UART Console** | 115200 baud (~11 KB/s) |
| **Log Messages/sec** | 100+ (queue-based) |

### Resource Utilization (Typical Operation)

| Resource | Usage | Peak |
|----------|-------|------|
| **CPU (Core 0)** | 15-25% | 40% |
| **RAM** | 160 KB | 180 KB |
| **Flash** | 210 KB | - |
| **FreeRTOS Heap** | 80 KB | 100 KB |

### Power Consumption

| State | Current | Power |
|-------|---------|-------|
| **Idle** | 40 mA | 200 mW @ 5V |
| **Normal Operation** | 80 mA | 400 mW @ 5V |
| **Peak (All Relays + Network)** | 120 mA | 600 mW @ 5V |

---

## Conclusion

The ENERGIS PDU firmware implements a robust, deterministic, and maintainable system using FreeRTOS. The architecture provides:

✅ **Reliability:** Task isolation, graceful degradation, comprehensive error handling  
✅ **Performance:** Deterministic timing, concurrent operations, efficient resource use  
✅ **Maintainability:** Clear ownership model, modular design, well-documented interfaces  
✅ **Scalability:** Easy to add new features, expand protocols, support more channels  

The system successfully meets all design requirements for a production-quality managed PDU.

---

**End of System Description**
