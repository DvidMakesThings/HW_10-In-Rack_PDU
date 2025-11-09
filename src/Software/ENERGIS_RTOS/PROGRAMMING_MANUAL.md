# ENERGIS PDU - Programming Manual

**Document Version:** 1.0  
**Author:** DvidMakesThings - David Sipos  
**Project:** ENERGIS - The Managed PDU Project for 10-Inch Rack  
**Date:** November 2025  
**Firmware Version:** 2.0.0

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Development Environment Setup](#development-environment-setup)
3. [Project Structure](#project-structure)
4. [Coding Standards](#coding-standards)
5. [RTOS Development Guidelines](#rtos-development-guidelines)
6. [Creating New Tasks](#creating-new-tasks)
7. [Adding New Drivers](#adding-new-drivers)
8. [Working with Shared Resources](#working-with-shared-resources)
9. [Logging System Usage](#logging-system-usage)
10. [Configuration Management](#configuration-management)
11. [Network Protocol Development](#network-protocol-development)
12. [Testing and Debugging](#testing-and-debugging)
13. [Common Pitfalls](#common-pitfalls)
14. [Build System](#build-system)
15. [API Reference](#api-reference)

---

## Getting Started

### Prerequisites

**Required Software:**
- **CMake** ≥ 3.13
- **ARM GCC Toolchain** ≥ 10.3.1
- **Pico SDK** ≥ 1.5.0
- **FreeRTOS Kernel** v11.0.0 (included)
- **Git** for version control
- **Serial Terminal** (minicom, PuTTY, etc.)

**Hardware:**
- ENERGIS PDU board
- USB cable (for programming and USB-CDC)
- UART adapter (optional, for console debugging)

### Quick Start

```bash
# Clone the repository
git clone https://github.com/DvidMakesThings/HW_10-In-Rack_PDU.git
cd HW_10-In-Rack_PDU

# Set up Pico SDK environment
export PICO_SDK_PATH=/path/to/pico-sdk

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j4

# Flash to device
# Hold BOOTSEL button, plug in USB, then:
cp ENERGIS_PDU.uf2 /media/$USER/RPI-RP2/
```

---

## Development Environment Setup

### CMakeLists.txt Configuration

The project uses CMake with the Pico SDK. Key configuration:

```cmake
cmake_minimum_required(VERSION 3.13)

# Include Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Include FreeRTOS
include(FreeRTOS_Kernel_import.cmake)

project(ENERGIS_PDU C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize SDK
pico_sdk_init()

# Add executable
add_executable(ENERGIS_PDU
    ENERGIS_RTOS.c
    # ... (all source files)
)

# Link libraries
target_link_libraries(ENERGIS_PDU
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_adc
    hardware_uart
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

# Enable USB output
pico_enable_stdio_usb(ENERGIS_PDU 1)
pico_enable_stdio_uart(ENERGIS_PDU 1)

# Create UF2 file
pico_add_extra_outputs(ENERGIS_PDU)
```

### VS Code Configuration

Recommended `settings.json`:

```json
{
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "cmake.configureOnOpen": true,
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "files.associations": {
        "*.h": "c",
        "freertos.h": "c",
        "task.h": "c",
        "queue.h": "c"
    },
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/**",
        "${env:PICO_SDK_PATH}/**"
    ]
}
```

---

## Project Structure

```
ENERGIS_PDU/
│
├── CMakeLists.txt              # Build configuration
├── FreeRTOSConfig.h            # RTOS configuration
├── CONFIG.h                    # Global configuration & includes
├── ENERGIS_RTOS.c              # Main entry point
├── serial_number.h             # Device serial number
│
├── tasks/                      # FreeRTOS task implementations
│   ├── InitTask.c/h           # System initialization
│   ├── LoggerTask.c/h         # Non-blocking logging
│   ├── ConsoleTask.c/h        # UART console
│   ├── NetTask.c/h            # Network management
│   ├── MeterTask.c/h          # Power metering
│   ├── StorageTask.c/h        # Configuration storage
│   └── ButtonTask.c/h         # Front panel interface
│
├── drivers/                    # Hardware drivers
│   ├── CAT24C512_driver.c/h   # EEPROM driver
│   ├── MCP23017_driver.c/h    # GPIO expander driver
│   ├── HLW8032_driver.c/h     # Power meter driver
│   ├── button_driver.c/h      # Button debouncing
│   ├── ethernet_driver.c/h    # W5500 low-level driver
│   ├── ethernet_config.h      # Ethernet configuration
│   ├── ethernet_w5500regs.h   # W5500 register definitions
│   ├── socket.c/h             # Socket API implementation
│   └── snmp.c/h               # SNMP protocol stack
│
├── snmp/                       # SNMP MIB modules
│   ├── snmp_custom.c/h        # Custom enterprise MIB
│   ├── snmp_networkCtrl.c/h   # Network control MIB
│   ├── snmp_outletCtrl.c/h    # Outlet control MIB
│   ├── snmp_powerMon.c/h      # Power monitoring MIB
│   └── snmp_voltageMon.c/h    # Voltage monitoring MIB
│
├── web_handlers/               # HTTP request handlers
│   ├── http_server.c/h        # HTTP server core
│   ├── page_content.c/h       # HTML page storage
│   ├── control_handler.c/h    # Outlet control API
│   ├── settings_handler.c/h   # Configuration API
│   └── status_handler.c/h     # Telemetry API
│
├── misc/                       # Miscellaneous utilities
│   ├── helpers.c/h            # Utility functions
│   └── EEPROM_MemoryMap.h     # EEPROM layout definitions
│
└── html/                       # Web interface sources
    ├── control.html           # Main control page
    ├── settings.html          # Configuration page
    ├── status.html            # Status/telemetry page
    ├── help.html              # Help documentation
    └── user_manual.html       # User manual
```

### File Organization Rules

1. **All tasks** go in `tasks/` directory
2. **All drivers** go in `drivers/` directory  
3. **Protocol implementations** get their own subdirectories (e.g., `snmp/`)
4. **Every .c file** must have a corresponding .h file
5. **CONFIG.h** is the ONLY file that includes other headers (centralized includes)

---

## Coding Standards

### File Header Format

**MANDATORY** for all files:

```c
/**
 * @file filename.c
 * @author DvidMakesThings - David Sipos
 * @defgroup GROUP_NAME - Group display name
 * @brief Brief description
 * @{
 *
 * @defgroup subgroup Subgroup title
 * @ingroup parent_group
 * @brief Subgroup description
 * @{
 * @version X.Y.Z
 * @date YYYY-MM-DD
 *
 * @details Detailed description
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */
```

### Function Documentation

**MANDATORY** Doxygen style for ALL functions:

```c
/**
 * @brief Brief description of what the function does
 * 
 * @details Optional detailed explanation of implementation,
 * algorithm, or important notes.
 * 
 * @param param1 Description of first parameter
 * @param param2 Description of second parameter
 * @return Description of return value
 * 
 * @note Any important notes or warnings
 * @warning Critical warnings about usage
 * 
 * @example
 * // Example usage:
 * result = my_function(10, 20);
 */
int my_function(int param1, int param2) {
    // Implementation
    return param1 + param2;
}
```

### Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| **Functions** | snake_case | `storage_get_network()` |
| **Variables** | snake_case | `relay_status`, `ch_count` |
| **Constants/Macros** | UPPER_SNAKE_CASE | `MAX_CHANNELS`, `I2C_SPEED` |
| **Types (struct/enum)** | snake_case_t | `meter_telemetry_t` |
| **Task Functions** | TaskName_Function | `MeterTask_Function()` |
| **Task Init** | TaskName_Init() | `NetTask_Init()` |
| **Task Status** | TaskName_IsReady() | `Storage_IsReady()` |
| **File Names** | snake_case | `power_meter.c` |

### Code Style

```c
// GOOD: Clear, readable code
void process_channel(uint8_t channel) {
    if (channel >= MAX_CHANNELS) {
        ERROR_PRINT("Invalid channel: %d\r\n", channel);
        return;
    }
    
    // Select channel and wait for settling
    hlw8032_select_channel(channel);
    vTaskDelay(pdMS_TO_TICKS(SETTLING_TIME_MS));
    
    // Read measurement
    hlw8032_data_t data;
    if (hlw8032_read_frame(&data)) {
        update_telemetry(channel, &data);
    }
}

// BAD: Hard to read, no error handling
void p(uint8_t c){if(c<8){s(c);vTaskDelay(10);r(&d);u(c,&d);}}
```

**Rules:**
- **Indentation:** 4 spaces (NO TABS)
- **Line Length:** 100 characters maximum
- **Braces:** K&R style (opening brace on same line)
- **Comments:** Explain WHY, not WHAT
- **Error Handling:** ALWAYS check return values

---

## RTOS Development Guidelines

### Core RTOS Principles

#### 1. **Replace Busy-Wait with vTaskDelay**

```c
// ❌ BAD - Wastes CPU
void old_delay(uint32_t ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start < ms) {
        tight_loop_contents();  // CPU at 100%!
    }
}

// ✅ GOOD - Yields to other tasks
void rtos_delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));  // CPU available for others
}
```

#### 2. **Use FreeRTOS Memory Management**

```c
// ❌ BAD - No tracking, potential leaks
char* buffer = malloc(1024);
if (buffer) {
    // Use buffer
    free(buffer);
}

// ✅ GOOD - FreeRTOS tracked allocation
char* buffer = pvPortMalloc(1024);
if (buffer) {
    // Use buffer
    vPortFree(buffer);
}
```

#### 3. **Protect Shared Resources with Mutexes**

```c
// ❌ BAD - Race condition!
void write_eeprom(uint16_t addr, uint8_t data) {
    CAT24C512_Write(addr, &data, 1);  // Two tasks could collide!
}

// ✅ GOOD - Mutex protected
extern SemaphoreHandle_t eepromMtx;

void write_eeprom(uint16_t addr, uint8_t data) {
    if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
        CAT24C512_Write(addr, &data, 1);
        xSemaphoreGive(eepromMtx);
    } else {
        ERROR_PRINT("EEPROM mutex timeout\r\n");
    }
}
```

#### 4. **Never Call Blocking Functions from ISRs**

```c
// ❌ BAD - ISR calling non-ISR-safe function
void gpio_isr(void) {
    vTaskDelay(10);  // FATAL ERROR! Never delay in ISR!
    xSemaphoreTake(mutex, 0);  // WRONG! Use FromISR version!
}

// ✅ GOOD - ISR only notifies task
static TaskHandle_t handler_task = NULL;

void gpio_isr(void) {
    BaseType_t higher_priority_woken = pdFALSE;
    vTaskNotifyGiveFromISR(handler_task, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

void handler_task_function(void* param) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Wait for ISR
        // Now handle the event in task context
        process_interrupt_event();
    }
}
```

#### 5. **One Owner Per Peripheral**

```c
// ❌ BAD - Multiple tasks touching same peripheral
void task_a(void* param) {
    w5500_send_data(socket, data, len);  // Collision!
}

void task_b(void* param) {
    w5500_receive_data(socket, buffer, len);  // Collision!
}

// ✅ GOOD - Single owner task
void NetTask_Function(void* param) {
    // Only NetTask touches W5500
    while (1) {
        w5500_send_data(socket, data, len);
        w5500_receive_data(socket, buffer, len);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Other tasks communicate via queues
void task_a(void* param) {
    net_msg_t msg = {.type = NET_SEND, .data = data, .len = len};
    xQueueSend(net_queue, &msg, portMAX_DELAY);
}
```

---

## Creating New Tasks

### Task Template

```c
/**
 * @file MyTask.c
 * @author DvidMakesThings - David Sipos
 * @defgroup tasks## ##. My Task
 * @ingroup tasks
 * @brief Description of what this task does
 * @{
 * @version 1.0.0
 * @date YYYY-MM-DD
 *
 * @details
 * Detailed description of:
 * - Task responsibilities
 * - Owned peripherals
 * - Communication interfaces
 * - Timing requirements
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* ===================== Configuration ===================== */
#define MYTASK_TAG "[MyTask]"
#define MYTASK_PRIORITY 3
#define MYTASK_STACK_SIZE 2048
#define MYTASK_LOOP_PERIOD_MS 100

/* ===================== Handles ===================== */
static TaskHandle_t myTaskHandle = NULL;

/* ===================== Ready Flag ===================== */
static volatile bool task_ready = false;

/**
 * @brief Check if MyTask is ready
 * @return true if initialized and operational
 */
bool MyTask_IsReady(void) {
    return task_ready;
}

/* ===================== Private Functions ===================== */

/**
 * @brief Initialize task-specific hardware/state
 * @return true on success, false on failure
 */
static bool mytask_init_hardware(void) {
    INFO_PRINT("%s Initializing hardware\r\n", MYTASK_TAG);
    
    // Initialize owned peripherals here
    // Example: Configure GPIO, I2C device, etc.
    
    return true;
}

/**
 * @brief Main processing logic
 */
static void mytask_process(void) {
    // Main task logic here
}

/* ===================== Task Function ===================== */

/**
 * @brief MyTask main function
 * @param pvParameters Unused
 */
static void MyTask_Function(void* pvParameters) {
    (void)pvParameters;
    
    INFO_PRINT("%s Starting\r\n", MYTASK_TAG);
    
    /* Initialize */
    if (!mytask_init_hardware()) {
        ERROR_PRINT("%s Hardware init failed\r\n", MYTASK_TAG);
        task_ready = false;
        vTaskDelete(NULL);  // Self-delete on fatal error
        return;
    }
    
    /* Signal ready */
    task_ready = true;
    INFO_PRINT("%s Ready\r\n", MYTASK_TAG);
    
    /* Main loop */
    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        mytask_process();
        
        /* Precise timing */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MYTASK_LOOP_PERIOD_MS));
    }
}

/* ===================== Public API ===================== */

/**
 * @brief Create and start MyTask
 * @param auto_start If true, create and start immediately
 * @return pdPASS on success, pdFAIL on error
 */
BaseType_t MyTask_Init(bool auto_start) {
    if (!auto_start) {
        return pdPASS;  // Deferred start
    }
    
    BaseType_t result = xTaskCreate(
        MyTask_Function,
        "MyTask",
        MYTASK_STACK_SIZE,
        NULL,
        MYTASK_PRIORITY,
        &myTaskHandle
    );
    
    if (result != pdPASS) {
        ERROR_PRINT("%s Failed to create task\r\n", MYTASK_TAG);
        return pdFAIL;
    }
    
    return pdPASS;
}

/** @} */
```

### Task Checklist

When creating a new task, ensure:

- [ ] Proper Doxygen header with @defgroup
- [ ] Unique priority (check existing tasks)
- [ ] Sufficient stack size (start with 2048, tune later)
- [ ] Ready flag (`volatile bool task_ready`)
- [ ] Status check function (`MyTask_IsReady()`)
- [ ] Init function (`MyTask_Init(bool auto_start)`)
- [ ] Hardware initialization in task context (not main)
- [ ] Error handling for all operations
- [ ] Proper use of vTaskDelay or vTaskDelayUntil
- [ ] Mutex protection for shared resources
- [ ] Logging with task tag (`[MyTask]`)
- [ ] Add to InitTask.c creation sequence
- [ ] Add to CMakeLists.txt
- [ ] Update CONFIG.h includes

---

## Adding New Drivers

### Driver Template

```c
/**
 * @file my_peripheral_driver.c
 * @author DvidMakesThings - David Sipos
 * @defgroup drivers_myperipheral My Peripheral Driver
 * @ingroup drivers
 * @brief Driver for MyPeripheral IC
 * @{
 * @version 1.0.0
 * @date YYYY-MM-DD
 *
 * @details
 * - Communication: I2C/SPI/UART
 * - Address/CS: 0xXX
 * - Owner Task: MyTask
 * - Thread Safety: Mutex protected / Exclusive
 * 
 * @project ENERGIS - The Managed PDU Project for 10-Inch Rack
 * @github https://github.com/DvidMakesThings/HW_10-In-Rack_PDU
 */

#include "../CONFIG.h"

/* ===================== Configuration ===================== */
#define MYPERIPHERAL_I2C i2c0
#define MYPERIPHERAL_ADDR 0x48
#define MYPERIPHERAL_TIMEOUT_MS 100

/* ===================== Private State ===================== */
static bool driver_initialized = false;

/* ===================== Private Functions ===================== */

/**
 * @brief Write register
 * @param reg Register address
 * @param value Value to write
 * @return true on success
 */
static bool write_register(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    int result = i2c_write_timeout_us(
        MYPERIPHERAL_I2C,
        MYPERIPHERAL_ADDR,
        buf,
        2,
        false,
        MYPERIPHERAL_TIMEOUT_MS * 1000
    );
    return (result == 2);
}

/**
 * @brief Read register
 * @param reg Register address
 * @param value Pointer to receive value
 * @return true on success
 */
static bool read_register(uint8_t reg, uint8_t* value) {
    int result = i2c_write_timeout_us(
        MYPERIPHERAL_I2C,
        MYPERIPHERAL_ADDR,
        &reg,
        1,
        true,  // Keep bus active
        MYPERIPHERAL_TIMEOUT_MS * 1000
    );
    if (result != 1) return false;
    
    result = i2c_read_timeout_us(
        MYPERIPHERAL_I2C,
        MYPERIPHERAL_ADDR,
        value,
        1,
        false,
        MYPERIPHERAL_TIMEOUT_MS * 1000
    );
    return (result == 1);
}

/* ===================== Public API ===================== */

/**
 * @brief Initialize the driver
 * @return true on success
 */
bool MyPeripheral_Init(void) {
    if (driver_initialized) {
        return true;  // Already initialized
    }
    
    // Device-specific initialization
    // Example: Configure registers, verify communication
    
    uint8_t device_id;
    if (!read_register(REG_DEVICE_ID, &device_id)) {
        ERROR_PRINT("[MyPeripheral] Communication failed\r\n");
        return false;
    }
    
    if (device_id != EXPECTED_DEVICE_ID) {
        ERROR_PRINT("[MyPeripheral] Wrong device ID: 0x%02X\r\n", device_id);
        return false;
    }
    
    driver_initialized = true;
    INFO_PRINT("[MyPeripheral] Initialized successfully\r\n");
    return true;
}

/**
 * @brief Perform device operation
 * @param param Operation parameter
 * @return Operation result
 */
int MyPeripheral_DoSomething(int param) {
    if (!driver_initialized) {
        ERROR_PRINT("[MyPeripheral] Not initialized\r\n");
        return -1;
    }
    
    // Implementation
    return 0;
}

/** @} */
```

### Driver Checklist

- [ ] Header file with public API
- [ ] Implementation file with private functions
- [ ] Doxygen documentation
- [ ] Initialization function
- [ ] Communication timeout handling
- [ ] Error checking on all operations
- [ ] Thread-safety consideration (mutex if shared)
- [ ] Clear ownership documented
- [ ] Add to CMakeLists.txt
- [ ] Add to CONFIG.h includes
- [ ] Test with actual hardware

---

## Working with Shared Resources

### Available Mutexes

```c
extern SemaphoreHandle_t i2cMtx;      // Protects I2C0 bus
extern SemaphoreHandle_t eepromMtx;   // Protects I2C1 + EEPROM
extern SemaphoreHandle_t spiMtx;      // Protects SPI0 bus
```

### Mutex Usage Pattern

```c
/**
 * @brief Example: Write to I2C device
 */
void write_to_display_mcp(uint8_t data) {
    // Take mutex with timeout
    if (xSemaphoreTake(i2cMtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Critical section - exclusive I2C0 access
        mcp_set_port(i2c0, MCP_DISPLAY_ADDR, 0, data);
        
        // ALWAYS release mutex!
        xSemaphoreGive(i2cMtx);
    } else {
        ERROR_PRINT("Failed to acquire i2cMtx\r\n");
        // Handle timeout - don't proceed with operation
    }
}
```

### Queue Communication Pattern

```c
/* ===================== Queue Definition ===================== */
typedef struct {
    uint8_t channel;
    float voltage;
    float current;
    float power;
} meter_msg_t;

QueueHandle_t q_meter_telemetry = NULL;

/* ===================== Producer (MeterTask) ===================== */
void publish_telemetry(uint8_t ch, float v, float i, float p) {
    meter_msg_t msg = {
        .channel = ch,
        .voltage = v,
        .current = i,
        .power = p
    };
    
    // Non-blocking send with timeout
    if (xQueueSend(q_meter_telemetry, &msg, pdMS_TO_TICKS(10)) != pdPASS) {
        WARNING_PRINT("Telemetry queue full, dropping sample\r\n");
    }
}

/* ===================== Consumer (NetTask) ===================== */
void process_telemetry(void) {
    meter_msg_t msg;
    
    // Non-blocking receive with timeout
    if (xQueueReceive(q_meter_telemetry, &msg, pdMS_TO_TICKS(100)) == pdPASS) {
        // Process message
        snmp_update_power_oid(msg.channel, msg.power);
        http_cache_telemetry(msg.channel, msg.voltage, msg.current, msg.power);
    }
}
```

---

## Logging System Usage

### Available Log Levels

```c
DEBUG_PRINT("Detailed debug info: value=%d\r\n", value);
INFO_PRINT("Normal operation message\r\n");
WARNING_PRINT("Non-critical issue detected\r\n");
ERROR_PRINT("Error condition: %s\r\n", error_msg);
NETLOG_PRINT("Network event: %s\r\n", event);
PLOT("time,%d,value,%f\r\n", timestamp, value);  // For plotting
ECHO("User command: %s\r\n", command);  // Console echo
```

### Logging Best Practices

```c
// ✅ GOOD: Structured, informative
INFO_PRINT("[MeterTask] Channel %d: V=%.1fV I=%.2fA P=%.1fW\r\n",
           channel, voltage, current, power);

// ✅ GOOD: Error with context
ERROR_PRINT("[NetTask] Socket %d send failed: errno=%d\r\n",
            socket, errno);

// ❌ BAD: No context
ERROR_PRINT("Error\r\n");

// ❌ BAD: Too verbose in loops
while (1) {
    DEBUG_PRINT("Loop iteration\r\n");  // Floods the log!
    vTaskDelay(pdMS_TO_TICKS(10));
}

// ✅ GOOD: Rate-limited logging
static uint32_t log_counter = 0;
while (1) {
    if (++log_counter % 100 == 0) {
        DEBUG_PRINT("Still running... (%lu iterations)\r\n", log_counter);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

### Conditional Compilation

Logging can be disabled at compile time:

```c
// In CONFIG.h
#define DEBUG 1    // Enable DEBUG_PRINT
#define INFO 1     // Enable INFO_PRINT
#define ERROR 1    // Enable ERROR_PRINT
#define WARNING 1  // Enable WARNING_PRINT
#define NETLOG 0   // Disable NETLOG_PRINT
```

---

## Configuration Management

### Reading Configuration

```c
/**
 * @brief Example: Read network configuration
 */
void configure_network(void) {
    networkInfo ni;
    
    // Wait for storage ready (if called early in boot)
    if (!storage_wait_ready(10000)) {
        ERROR_PRINT("Storage not ready, using defaults\r\n");
        return;
    }
    
    // Read network config from storage
    if (storage_get_network(&ni)) {
        INFO_PRINT("IP: %d.%d.%d.%d\r\n",
                   ni.ip[0], ni.ip[1], ni.ip[2], ni.ip[3]);
        
        // Apply configuration
        w5500_set_network_config(&ni);
    } else {
        ERROR_PRINT("Failed to read network config\r\n");
    }
}
```

### Updating Configuration

```c
/**
 * @brief Example: Update network configuration
 */
void update_ip_address(uint8_t new_ip[4]) {
    networkInfo ni;
    
    // Read current configuration
    if (!storage_get_network(&ni)) {
        ERROR_PRINT("Failed to read current config\r\n");
        return;
    }
    
    // Modify configuration
    memcpy(ni.ip, new_ip, 4);
    
    // Write back (will be debounced and written to EEPROM)
    if (storage_set_network(&ni)) {
        INFO_PRINT("Network config updated\r\n");
    } else {
        ERROR_PRINT("Failed to update network config\r\n");
    }
}
```

### StorageTask API Reference

```c
/* Network Configuration */
bool storage_get_network(networkInfo* ni);
bool storage_set_network(const networkInfo* ni);

/* Relay Status */
bool storage_get_relay(uint8_t channel, uint8_t* state);
bool storage_set_relay(uint8_t channel, uint8_t state);
bool storage_get_relay_mask(uint8_t* mask);
bool storage_set_relay_mask(uint8_t mask);

/* User Preferences */
bool storage_get_user_prefs(userPrefInfo* prefs);
bool storage_set_user_prefs(const userPrefInfo* prefs);

/* Energy Data */
bool storage_get_energy(uint8_t channel, float* energy_kwh);
bool storage_set_energy(uint8_t channel, float energy_kwh);

/* System */
bool storage_wait_ready(uint32_t timeout_ms);
bool Storage_IsReady(void);
```

---

## Network Protocol Development

### Adding HTTP Endpoints

1. **Define URI in page_content.h:**

```c
#define URI_MY_API "/api/myendpoint"
```

2. **Create handler function:**

```c
/**
 * @brief Handle /api/myendpoint requests
 * @param socket Socket number
 * @param method HTTP method (GET/POST)
 * @param uri Request URI
 * @param body Request body (for POST)
 * @param body_len Length of body
 */
void handle_my_api(uint8_t socket, const char* method,
                   const char* uri, const char* body, size_t body_len) {
    if (strcmp(method, "GET") == 0) {
        // Handle GET request
        char response[256];
        snprintf(response, sizeof(response),
                 "{\"status\":\"ok\",\"value\":%d}", get_some_value());
        
        http_send_json_response(socket, 200, response);
        
    } else if (strcmp(method, "POST") == 0) {
        // Handle POST request
        // Parse JSON body, update configuration, etc.
        
        http_send_json_response(socket, 200, "{\"status\":\"updated\"}");
        
    } else {
        http_send_error(socket, 405, "Method Not Allowed");
    }
}
```

3. **Register in http_server.c:**

```c
void http_route_request(uint8_t socket, const char* method, const char* uri) {
    if (strcmp(uri, URI_MY_API) == 0) {
        handle_my_api(socket, method, uri, body, body_len);
    } else if (...) {
        // Other routes
    }
}
```

### Adding SNMP OIDs

1. **Define OID in snmp_custom.h:**

```c
#define OID_MY_VALUE  {1,3,6,1,4,1,99999,1,5,1}
#define OID_MY_VALUE_LEN  10
```

2. **Implement getter/setter:**

```c
/**
 * @brief Get custom OID value
 */
int snmp_get_my_value(uint8_t* oid, uint8_t oid_len,
                      uint8_t* response, size_t* response_len) {
    int value = get_my_value();
    
    // Encode as integer
    response[0] = ASN1_INTEGER;
    response[1] = 4;  // Length
    response[2] = (value >> 24) & 0xFF;
    response[3] = (value >> 16) & 0xFF;
    response[4] = (value >> 8) & 0xFF;
    response[5] = value & 0xFF;
    
    *response_len = 6;
    return SNMP_ERR_NO_ERROR;
}

/**
 * @brief Set custom OID value
 */
int snmp_set_my_value(uint8_t* oid, uint8_t oid_len,
                      uint8_t* value, size_t value_len) {
    // Parse integer from value
    int new_value = (value[2] << 24) | (value[3] << 16) |
                    (value[4] << 8) | value[5];
    
    // Update value
    set_my_value(new_value);
    
    return SNMP_ERR_NO_ERROR;
}
```

3. **Register in SNMP handler:**

```c
void snmp_process_get(uint8_t* oid, uint8_t oid_len) {
    if (oid_matches(oid, oid_len, OID_MY_VALUE, OID_MY_VALUE_LEN)) {
        snmp_get_my_value(oid, oid_len, response, &response_len);
    } else {
        // Other OIDs
    }
}
```

---

## Testing and Debugging

### Serial Console Commands

Connect UART1 (115200 baud) or USB-CDC:

```
# System status
> status
System Status:
  Uptime: 00:12:34
  Free Heap: 45678 bytes
  Tasks Running: 6
  
# Network info
> netinfo
Network Configuration:
  IP: 192.168.0.11
  Subnet: 255.255.255.0
  Gateway: 192.168.0.1
  MAC: 00:08:DC:BE:EF:91
  Link: Up (100 Mbps)

# Control outlet
> outlet 1 on
Outlet 1: ON

# View power
> power 1
Channel 1:
  Voltage: 230.5 V
  Current: 1.25 A
  Power: 288.1 W
  Energy: 0.145 kWh

# Reboot
> reboot
Rebooting...

# Enter bootloader
> bootsel
Entering bootloader mode...
```

### Debug Logging

Enable verbose debugging:

```c
// In CONFIG.h
#define DEBUG 1
#define INFO 1
#define ERROR 1
#define WARNING 1
#define NETLOG 1
```

### Stack Usage Monitoring

```c
/**
 * @brief Print task stack high water marks
 */
void print_stack_usage(void) {
    INFO_PRINT("Task Stack Usage:\r\n");
    INFO_PRINT("  InitTask: %lu bytes free\r\n",
               uxTaskGetStackHighWaterMark(initTaskHandle) * 4);
    INFO_PRINT("  LoggerTask: %lu bytes free\r\n",
               uxTaskGetStackHighWaterMark(loggerTaskHandle) * 4);
    // ... for each task
}
```

### Heap Usage Monitoring

```c
/**
 * @brief Print heap statistics
 */
void print_heap_usage(void) {
    size_t free_heap = xPortGetFreeHeapSize();
    size_t min_ever_free = xPortGetMinimumEverFreeHeapSize();
    
    INFO_PRINT("Heap Usage:\r\n");
    INFO_PRINT("  Current free: %lu bytes\r\n", free_heap);
    INFO_PRINT("  Minimum ever: %lu bytes\r\n", min_ever_free);
    INFO_PRINT("  Used: %lu bytes\r\n",
               configTOTAL_HEAP_SIZE - free_heap);
}
```

### Common Debug Scenarios

#### Task Not Starting

```c
// Check if task creation succeeded
BaseType_t result = xTaskCreate(...);
if (result != pdPASS) {
    ERROR_PRINT("Failed to create task - likely out of heap\r\n");
    print_heap_usage();
}
```

#### Mutex Deadlock

```c
// Always use timeouts, never infinite wait
if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    ERROR_PRINT("Mutex timeout - potential deadlock\r\n");
    // Don't proceed with operation
    return;
}
```

#### Queue Overflow

```c
// Check queue status
UBaseType_t queue_items = uxQueueMessagesWaiting(q_telemetry);
UBaseType_t queue_spaces = uxQueueSpacesAvailable(q_telemetry);

if (queue_spaces == 0) {
    WARNING_PRINT("Telemetry queue full - consumer too slow\r\n");
}
```

---

## Common Pitfalls

### 1. **Forgetting to Release Mutex**

```c
// ❌ WRONG - Mutex leak on error path
if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
    if (!CAT24C512_Write(addr, data, len)) {
        return false;  // BUG: Forgot to release mutex!
    }
    xSemaphoreGive(eepromMtx);
}

// ✅ CORRECT - Always release, even on error
if (xSemaphoreTake(eepromMtx, pdMS_TO_TICKS(1000)) == pdTRUE) {
    bool success = CAT24C512_Write(addr, data, len);
    xSemaphoreGive(eepromMtx);  // ALWAYS release
    return success;
}
```

### 2. **ISR-Unsafe Functions in ISR**

```c
// ❌ WRONG - Non-ISR-safe function in ISR
void button_isr(void) {
    xQueueSend(button_queue, &data, 0);  // WRONG!
}

// ✅ CORRECT - Use FromISR version
void button_isr(void) {
    BaseType_t higher_priority_woken = pdFALSE;
    xQueueSendFromISR(button_queue, &data, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}
```

### 3. **Stack Overflow**

```c
// ❌ WRONG - Large buffer on stack
void process_data(void) {
    char large_buffer[4096];  // Too big for stack!
    sprintf(large_buffer, ...);
}

// ✅ CORRECT - Use heap or static
void process_data(void) {
    char* buffer = pvPortMalloc(4096);
    if (buffer) {
        sprintf(buffer, ...);
        vPortFree(buffer);
    }
}
```

### 4. **Shared Resource Without Mutex**

```c
// ❌ WRONG - Two tasks touching same I2C without protection
void task_a(void) {
    i2c_write(i2c0, 0x20, data, len);  // RACE CONDITION!
}

void task_b(void) {
    i2c_read(i2c0, 0x21, buffer, len);  // RACE CONDITION!
}

// ✅ CORRECT - Mutex protection
void task_a(void) {
    xSemaphoreTake(i2cMtx, portMAX_DELAY);
    i2c_write(i2c0, 0x20, data, len);
    xSemaphoreGive(i2cMtx);
}
```

### 5. **Busy-Wait Instead of vTaskDelay**

```c
// ❌ WRONG - Wastes CPU, blocks other tasks
void wait_for_data(void) {
    while (!data_ready()) {
        // Spinning at 100% CPU!
    }
}

// ✅ CORRECT - Yields to other tasks
void wait_for_data(void) {
    while (!data_ready()) {
        vTaskDelay(pdMS_TO_TICKS(10));  // Others can run
    }
}
```

---

## Build System

### CMake Configuration

The project uses CMake with Pico SDK:

```cmake
# Minimum CMake version
cmake_minimum_required(VERSION 3.13)

# Set toolchain
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)

# Include SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)

# Project definition
project(ENERGIS_PDU C CXX ASM)

# Standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize SDK
pico_sdk_init()

# Source files
set(SOURCES
    ENERGIS_RTOS.c
    tasks/InitTask.c
    tasks/LoggerTask.c
    # ... (all .c files)
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tasks
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/misc
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_uart
    hardware_adc
    hardware_gpio
    hardware_watchdog
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

# Enable USB and UART output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

# Generate UF2 file
pico_add_extra_outputs(${PROJECT_NAME})
```

### Building

```bash
# Clean build
rm -rf build && mkdir build && cd build

# Configure
cmake ..

# Build (parallel)
make -j$(nproc)

# Output files
ls -lh ENERGIS_PDU.*
# ENERGIS_PDU.elf - ELF binary (for debugging)
# ENERGIS_PDU.uf2 - UF2 file (for flashing)
# ENERGIS_PDU.bin - Raw binary
# ENERGIS_PDU.hex - Intel HEX format
```

### Flashing

```bash
# Method 1: UF2 bootloader (easiest)
# 1. Hold BOOTSEL button
# 2. Plug in USB
# 3. Copy UF2 file:
cp ENERGIS_PDU.uf2 /media/$USER/RPI-RP2/

# Method 2: OpenOCD + SWD (for debugging)
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
        -c "program ENERGIS_PDU.elf verify reset exit"

# Method 3: picotool
picotool load ENERGIS_PDU.uf2
picotool reboot
```

---

## API Reference

### Task Initialization

```c
/* InitTask */
void InitTask_Create(void);

/* LoggerTask */
void LoggerTask_Init(bool auto_start);
bool Logger_IsReady(void);
void log_printf(const char* format, ...);

/* ConsoleTask */
void ConsoleTask_Init(bool auto_start);
bool Console_IsReady(void);

/* StorageTask */
void StorageTask_Init(bool auto_start);
bool Storage_IsReady(void);
bool storage_wait_ready(uint32_t timeout_ms);

/* NetTask */
BaseType_t NetTask_Init(bool auto_start);
bool Net_IsReady(void);

/* MeterTask */
BaseType_t MeterTask_Init(bool auto_start);
bool Meter_IsReady(void);

/* ButtonTask */
void ButtonTask_Init(bool auto_start);
bool Button_IsReady(void);
```

### Storage API

```c
/* Network Configuration */
bool storage_get_network(networkInfo* ni);
bool storage_set_network(const networkInfo* ni);

/* Relay Control */
bool storage_get_relay(uint8_t channel, uint8_t* state);
bool storage_set_relay(uint8_t channel, uint8_t state);
bool storage_get_relay_mask(uint8_t* mask);
bool storage_set_relay_mask(uint8_t mask);

/* User Preferences */
bool storage_get_user_prefs(userPrefInfo* prefs);
bool storage_set_user_prefs(const userPrefInfo* prefs);

/* Energy Data */
bool storage_get_energy(uint8_t channel, float* energy_kwh);
bool storage_set_energy(uint8_t channel, float energy_kwh);
```

### Driver APIs

#### CAT24C512 EEPROM
```c
void CAT24C512_Init(void);
bool CAT24C512_Write(uint16_t addr, const uint8_t* data, size_t len);
bool CAT24C512_Read(uint16_t addr, uint8_t* data, size_t len);
bool CAT24C512_WritePage(uint16_t addr, const uint8_t* data);
```

#### MCP23017 GPIO Expander
```c
void MCP2017_Init(void);
void mcp_set_pin(i2c_inst_t* i2c, uint8_t addr, uint8_t pin, bool state);
bool mcp_get_pin(i2c_inst_t* i2c, uint8_t addr, uint8_t pin);
void mcp_set_port(i2c_inst_t* i2c, uint8_t addr, uint8_t port, uint8_t value);
uint8_t mcp_get_port(i2c_inst_t* i2c, uint8_t addr, uint8_t port);
```

#### HLW8032 Power Meter
```c
void HLW8032_Init(void);
bool HLW8032_ReadFrame(uint8_t* frame);
bool HLW8032_ParseData(const uint8_t* frame, hlw8032_data_t* data);
void HLW8032_SelectChannel(uint8_t channel);
```

#### W5500 Ethernet
```c
bool w5500_hw_init(void);
bool w5500_chip_init(void);
void w5500_set_network_config(const w5500_NetConfig* cfg);
uint8_t w5500_get_link_status(void);
void w5500_print_network(void);
```

#### Socket API
```c
int8_t socket_open(uint8_t sn, uint8_t protocol, uint16_t port);
int8_t socket_close(uint8_t sn);
int8_t socket_listen(uint8_t sn);
int32_t socket_send(uint8_t sn, const uint8_t* buf, uint16_t len);
int32_t socket_recv(uint8_t sn, uint8_t* buf, uint16_t len);
uint8_t socket_get_status(uint8_t sn);
```

### Utility Functions

#### LED Control
```c
void setPowerGood(bool state);        // Green LED
void setNetworkLink(bool state);      // Blue LED
void setError(bool state);            // Red LED
void setOutletLED(uint8_t ch, bool state);      // Outlet LEDs
void setSelectionLED(uint8_t ch, bool state);   // Selection LEDs
```

#### Helper Functions
```c
uint8_t calculate_crc8(const uint8_t* data, size_t len);
bool validate_ip_address(const uint8_t ip[4]);
bool validate_mac_address(const uint8_t mac[6]);
uint32_t get_uptime_seconds(void);
```

---

## Conclusion

This programming manual provides the essential information for developing on the ENERGIS PDU platform. Key takeaways:

1. **Always follow RTOS guidelines** - Use vTaskDelay, mutexes, and proper initialization
2. **Respect peripheral ownership** - One task per peripheral, communicate via queues
3. **Document everything** - Doxygen headers are mandatory, not optional
4. **Test incrementally** - Add one feature at a time, verify with hardware
5. **Check return values** - ALWAYS handle errors gracefully

For questions or issues, refer to:
- System Description document (hardware/architecture details)
- RTOS Migration document (rationale and patterns)
- FreeRTOS documentation (https://www.freertos.org/Documentation)
- Pico SDK documentation (https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf)

---

**End of Programming Manual**
