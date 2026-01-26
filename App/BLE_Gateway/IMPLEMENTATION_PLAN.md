# STM32WB BLE Gateway - Implementation Plan

---

## Table of Contents

1. [System Architecture](#system-architecture)
2. [STM32_WPAN API Analysis](#stm32_wpan-api-analysis)
3. [Current Implementation Status](#current-implementation-status)
4. [Required Implementations](#required-implementations)
5. [Implementation Workflow](#implementation-workflow)
6. [AT Command Protocol](#at-command-protocol)
7. [Directory Structure](#directory-structure)
8. [API Usage Examples](#api-usage-examples)
9. [Error Handling](#error-handling)
10. [Memory Considerations](#memory-considerations)
11. [Testing Checklist](#testing-checklist)

---

## System Architecture

### Overview

This project implements a BLE Central Gateway (Hub) that manages multiple BLE devices through an AT command interface. The system supports up to 8 simultaneous BLE connections with full GATT client capabilities.

**Current Status:** Phase 1 Complete + Optimized

### Communication Channels

- **LPUART1** (921600 baud) - AT Command interface for RX/TX data only
- **USB CDC** - Debug output via printf() - NO AT COMMANDS
- **BLE Stack** - Multi-device GATT Client operations

### Folder Structure

```
STM32_WB55_BLE_V2/
├── App/
│   └── BLE_Gateway/          [Custom application modules]
│       ├── Inc/
│       │   ├── at_command.h
│       │   ├── ble_device_manager.h
│       │   ├── ble_connection.h
│       │   ├── ble_gatt_client.h       [Optimized - write/notify only]
│       │   ├── ble_event_handler.h
│       │   ├── debug_trace.h
│       │   └── module_execute.h        [Application entry point]
│       └── Src/
│           ├── at_command.c
│           ├── ble_device_manager.c
│           ├── ble_connection.c
│           ├── ble_gatt_client.c       [Optimized - removed unused code]
│           ├── ble_event_handler.c
│           ├── debug_trace.c
│           └── module_execute.c        [Init + main loop handler]
├── Inc/                       [STM32CubeMX generated]
│   ├── app_ble.h
│   ├── p2p_client_app.h
│   └── main.h
├── Src/                       [STM32CubeMX generated]
│   ├── app_ble.c              [Modified - integrate Gateway modules]
│   ├── p2p_client_app.c       [Modified - scan callback]
│   ├── main.c                 [Modified - USB CDC printf]
│   └── usbd_cdc_if.c          [Modified - CDC data callback]
└── CMakeLists.txt             [Modified - add App/BLE_Gateway sources]
```

---

## STM32_WPAN API Analysis

### 1. HCI Layer APIs (ble_hci_le.h)

Low-level Host Controller Interface functions.

#### A. Connection Management

```c
// Disconnect from device
tBleStatus hci_disconnect(uint16_t conn_handle, uint8_t reason);

// Read remote device version
tBleStatus hci_read_remote_version_information(uint16_t conn_handle);

// Read transmit power level
tBleStatus hci_read_transmit_power_level(uint16_t conn_handle, 
                                         uint8_t type, 
                                         int8_t *power);
```

#### B. Controller Configuration

```c
// Reset BLE controller
tBleStatus hci_reset(void);

// Configure event mask
tBleStatus hci_set_event_mask(uint8_t event_mask[8]);

// Configure flow control
tBleStatus hci_set_controller_to_host_flow_control(uint8_t flow_ctrl_enable);
```

#### C. LE Scanning and Connection

**Critical Functions:**

```c
// Configure scan parameters
tBleStatus hci_le_set_scan_parameters(uint8_t scan_type,
                                      uint16_t scan_interval,
                                      uint16_t scan_window,
                                      uint8_t own_addr_type,
                                      uint8_t filter_policy);

// Start/stop scanning
tBleStatus hci_le_set_scan_enable(uint8_t enable, uint8_t filter_duplicates);

// Initiate connection to device
tBleStatus hci_le_create_connection(uint16_t scan_interval,
                                    uint16_t scan_window,
                                    uint8_t filter_policy,
                                    uint8_t peer_addr_type,
                                    const uint8_t peer_addr[6],
                                    uint8_t own_addr_type,
                                    uint16_t conn_interval_min,
                                    uint16_t conn_interval_max,
                                    uint16_t conn_latency,
                                    uint16_t supervision_timeout,
                                    uint16_t min_ce_length,
                                    uint16_t max_ce_length);

// Cancel connection attempt
tBleStatus hci_le_create_connection_cancel(void);

// Configure PHY (1M, 2M, Coded)
tBleStatus hci_le_set_default_phy(uint8_t all_phys,
                                  uint8_t tx_phys,
                                  uint8_t rx_phys);
```

### 2. GAP APIs (ble_gap_aci.h)

Generic Access Profile functions for device discovery and connection.

#### A. Discovery and Advertising (Peripheral role)

```c
// Disable advertising
tBleStatus aci_gap_set_non_discoverable(void);

// Limited discoverable mode
tBleStatus aci_gap_set_limited_discoverable(uint8_t adv_type, ...);

// General discoverable mode
tBleStatus aci_gap_set_discoverable(uint8_t adv_type, ...);
```

#### B. Connection Management (Central role)

```c
// Start GAP procedure (scan, connect, etc.)
tBleStatus aci_gap_start_procedure(uint8_t proc_code, ...);

// Terminate GAP procedure
tBleStatus aci_gap_terminate_proc(uint8_t proc_code);

// Create connection
tBleStatus aci_gap_create_connection(uint8_t init_filter_policy,
                                     uint8_t own_addr_type,
                                     uint8_t peer_addr_type,
                                     const uint8_t peer_addr[6],
                                     ...);

// Terminate connection
tBleStatus aci_gap_terminate(uint16_t conn_handle, uint8_t reason);
```

#### C. Device Information

```c
// Get BLE address
tBleStatus aci_gap_get_bdaddr(uint8_t *addr_type, uint8_t addr[6]);

// Update advertising data
tBleStatus aci_gap_update_adv_data(uint8_t adv_data_len, 
                                   const uint8_t *adv_data);

// Get security level
tBleStatus aci_gap_get_security_level(uint16_t conn_handle, 
                                      uint8_t *security_level);
```

### 3. GATT APIs (ble_gatt_aci.h)

#### A. GATT Initialization

```c
// MUST CALL FIRST - Initialize GATT layer
tBleStatus aci_gatt_init(void);
```

#### B. Service Management (Server)

```c
// Add service to server
tBleStatus aci_gatt_add_service(uint8_t service_uuid_type,
                                const uint8_t *service_uuid,
                                uint8_t service_type,
                                uint8_t max_attr_records,
                                uint16_t *service_handle);

// Add characteristic to service
tBleStatus aci_gatt_add_char(uint16_t service_handle,
                             uint8_t char_uuid_type,
                             const uint8_t *char_uuid,
                             uint8_t char_value_len,
                             uint8_t char_properties,
                             uint8_t sec_permissions,
                             uint8_t gatt_evt_mask,
                             uint8_t enc_key_size,
                             uint8_t is_variable,
                             uint16_t *char_handle);

// Delete characteristic
tBleStatus aci_gatt_del_char(uint16_t service_handle, 
                             uint16_t char_handle);

// Delete service
tBleStatus aci_gatt_del_service(uint16_t service_handle);
```

#### C. Characteristic Read/Write (Server)

```c
// Update characteristic value
tBleStatus aci_gatt_update_char_value(uint16_t service_handle,
                                      uint16_t char_handle,
                                      uint8_t val_offset,
                                      uint8_t char_value_len,
                                      const uint8_t *char_value);

// Configure event mask
tBleStatus aci_gatt_set_event_mask(uint32_t event_mask);
```

#### D. GATT Client Operations (Central role - CRITICAL)

```c
// Read characteristic value
tBleStatus aci_gattc_read_char_value(uint16_t conn_handle, 
                                     uint16_t char_handle);

// Write characteristic value
tBleStatus aci_gattc_write_char_value(uint16_t conn_handle,
                                      uint16_t char_handle,
                                      uint8_t value_len,
                                      const uint8_t *value);

// Write descriptor (enable notifications)
tBleStatus aci_gattc_write_char_desc(uint16_t conn_handle,
                                     uint16_t char_handle,
                                     uint8_t value_len,
                                     const uint8_t *value);

// Discover all services
tBleStatus aci_gattc_discover_all_services(uint16_t conn_handle);

// Discover specific primary service by UUID
tBleStatus aci_gattc_discover_primary_services(uint16_t conn_handle,
                                               uint8_t uuid_type,
                                               const uint8_t *uuid);

// Discover all characteristics in service
tBleStatus aci_gattc_discover_all_chars(uint16_t conn_handle,
                                        uint16_t start_handle,
                                        uint16_t end_handle);

// Discover all characteristic descriptors
tBleStatus aci_gattc_discover_all_char_desc(uint16_t conn_handle,
                                            uint16_t start_handle,
                                            uint16_t end_handle);
```

### 4. HCI Events (ble_events.h)

#### A. Connection Events

```c
// Device connected successfully
void hci_le_connection_complete_event(uint8_t status,
                                      uint16_t conn_handle,
                                      uint8_t role,
                                      uint8_t peer_addr_type,
                                      const uint8_t peer_addr[6],
                                      uint16_t conn_interval,
                                      uint16_t conn_latency,
                                      uint16_t supervision_timeout,
                                      uint8_t master_clock_accuracy);

// Device disconnected
void hci_disconnection_complete_event(uint8_t status,
                                      uint16_t conn_handle,
                                      uint8_t reason);

// Enhanced connection complete (BLE 4.2+)
void hci_le_enhanced_connection_complete_event(...);
```

#### B. Scanning Events

```c
// CRITICAL - Receive advertising packet during scan
void hci_le_advertising_report_event(uint8_t num_reports,
                                     const hci_le_advertising_report_event_t *reports);
// Extract:
//   - MAC address from reports->address
//   - RSSI from reports->rssi
//   - Advertising data from reports->data
```

#### C. GATT Events

```c
// Receive notification from remote device
void hci_le_gattc_notification_event(uint16_t conn_handle,
                                     uint16_t event_data_len,
                                     const uint8_t *event_data);

// Read characteristic response
void hci_le_gattc_read_response(uint16_t conn_handle,
                                uint16_t event_data_len,
                                const uint8_t *event_data);

// Write characteristic response
void hci_le_gattc_write_response(uint16_t conn_handle);

// Discover services response
void hci_le_gattc_disc_read_char_by_uuid_resp(uint16_t conn_handle,
                                              uint16_t event_data_len,
                                              const uint8_t *event_data);
```

#### D. Encryption Events

```c
// Encryption state changed
void hci_encryption_change_event(uint8_t status,
                                 uint16_t conn_handle,
                                 uint8_t encryption_enabled);

// Encryption key refreshed
void hci_encryption_key_refresh_complete_event(uint8_t status,
                                               uint16_t conn_handle);
```

---

## Current Implementation Status

### Existing Files

```
Inc/
  app_ble.h          - BLE application header
  p2p_client_app.h   - P2P Client template
  
Src/
  app_ble.c          - BLE initialization and callbacks
  p2p_client_app.c   - P2P Client state machine
  main.c             - Main loop
```

### Existing Enums

```c
// BLE connection states
typedef enum {
    APP_BLE_IDLE,
    APP_BLE_FAST_ADV,
    APP_BLE_LP_ADV,
    APP_BLE_SCAN,
    APP_BLE_LP_CONNECTING,
    APP_BLE_CONNECTED_SERVER,
    APP_BLE_CONNECTED_CLIENT,
    APP_BLE_DISCOVER_SERVICES,
    APP_BLE_DISCOVER_CHARACS,
    APP_BLE_DISCOVER_WRITE_DESC,
    APP_BLE_DISCOVER_NOTIFICATION_CHAR_DESC,
    APP_BLE_ENABLE_NOTIFICATION_DESC,
    APP_BLE_DISABLE_NOTIFICATION_DESC
} APP_BLE_ConnStatus_t;

// P2P Client event opcodes
typedef enum {
    PEER_CONN_HANDLE_EVT,
    PEER_DISCON_HANDLE_EVT
} P2PC_APP_Opcode_Notification_evt_t;
```

---

## Required Implementations

### TIER 1: CRITICAL (Must Implement Immediately)

#### 1.1 UART AT Command Parser Module

**Files:** `App/BLE_Gateway/Inc/at_command.h` + `App/BLE_Gateway/Src/at_command.c`

**CRITICAL:** UART is only for AT commands, NOT for printf debug output!

**Required API:**

```c
// AT Command Handler
void AT_Command_Init(void);
void AT_Command_Process(const char *cmd_line);
void AT_Command_UART_IRQHandler(void);

// Command handlers
int AT_SCAN_Handler(void);                          // AT+SCAN
int AT_CONNECT_Handler(const uint8_t *mac);        // AT+CONNECT=XX:XX:XX:XX:XX:XX
int AT_DISCONNECT_Handler(uint8_t conn_idx);       // AT+DISCONNECT=<idx>
int AT_LIST_Handler(void);                          // AT+LIST (list connected devices)
int AT_READ_Handler(uint8_t conn_idx, uint16_t handle);  // AT+READ=<idx>,<handle>
int AT_WRITE_Handler(uint8_t conn_idx, uint16_t handle, const uint8_t *data); // AT+WRITE
int AT_NOTIFY_Handler(uint8_t conn_idx, uint16_t handle, uint8_t enable); // AT+NOTIFY

// UART TX only for responses (NO printf here!)
void UART_SendATResponse(const char *fmt, ...);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
```

**Features:**
- Parse AT command strings from LPUART1
- Validate command syntax
- Return OK/ERROR via UART TX
- Use circular buffer for UART RX
- NO printf usage (only HAL_UART_Transmit)

#### 1.2 BLE Device Manager Module

**Files:** `App/BLE_Gateway/Inc/ble_device_manager.h` + `App/BLE_Gateway/Src/ble_device_manager.c`

**Data Structures:**

```c
typedef struct {
    uint8_t   mac_addr[6];              // MAC address
    uint16_t  conn_handle;              // Connection handle (-1 if not connected)
    uint8_t   is_connected;             // Connection status
    int8_t    rssi;                     // Last RSSI value
    uint8_t   service_count;            // Number of discovered services
    // Service and characteristic data...
} BLE_Device_t;

// Manager structure
typedef struct {
    BLE_Device_t devices[8];            // Max 8 devices
    uint8_t      active_count;          // Current device count
    uint8_t      scan_active;           // Is scan running?
    uint16_t     scan_duration;         // Duration in ms
} BLE_DeviceManager_t;
```

**Required API:**

```c
void BLE_DeviceManager_Init(void);
void BLE_DeviceManager_AddDevice(const uint8_t *mac, int8_t rssi);
int  BLE_DeviceManager_FindDevice(const uint8_t *mac);
int  BLE_DeviceManager_FindConnHandle(uint16_t conn_handle);
void BLE_DeviceManager_UpdateConnection(uint16_t conn_handle, uint8_t connected);
void BLE_DeviceManager_PrintList(void);
BLE_Device_t* BLE_DeviceManager_GetDevice(int idx);
void BLE_DeviceManager_Clear(void);
```

#### 1.3 Modify Scan Callback (P2P Client)

**File:** `Src/p2p_client_app.c` - Function `hci_le_advertising_report_event()`

**Change from auto-connect to scan-only:**

```c
// BEFORE (auto-connect):
// void hci_le_advertising_report_event(...) {
//     aci_gap_create_connection(...);  // Auto connect
// }

// AFTER (scan only, no auto-connect):
void hci_le_advertising_report_event(uint8_t Num_Reports,
                                     const hci_le_advertising_report_event_t *reports) {
    for(int i = 0; i < Num_Reports; i++) {
        const hci_le_advertising_report_event_t *report = &reports[i];
        
        // Extract MAC address
        uint8_t *mac = report->Address;
        int8_t rssi = report->RSSI;
        
        // Add to device manager
        BLE_DeviceManager_AddDevice(mac, rssi);
        
        // Print to UART (AT response)
        printf("+SCAN_DEVICE:%02X:%02X:%02X:%02X:%02X:%02X,RSSI:%d\r\n",
               mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], rssi);
    }
}
```

#### 1.4 Multi-Device Connection Manager

**Files:** `App/BLE_Gateway/Inc/ble_connection.h` + `App/BLE_Gateway/Src/ble_connection.c`

**Features:**

```c
// Connection state tracking
typedef enum {
    CONN_STATE_IDLE,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_DISCOVERING,
    CONN_STATE_DISCONNECTING
} BLE_ConnectionState_t;

// API
int  BLE_Connection_CreateConnection(const uint8_t *mac);
int  BLE_Connection_TerminateConnection(uint16_t conn_handle);
int  BLE_Connection_SetState(uint16_t conn_handle, BLE_ConnectionState_t state);
BLE_ConnectionState_t BLE_Connection_GetState(uint16_t conn_handle);
```

#### 1.5 GATT Client Write and Notify (OPTIMIZED)

**Files:** `App/BLE_Gateway/Inc/ble_gatt_client.h` + `App/BLE_Gateway/Src/ble_gatt_client.c`

**Note:** Discovery and read operations have been removed as they were not implemented and not used.

**API (Simplified):**

```c
// Write/Notify only - actively used operations
void BLE_GATT_Init(void);
int BLE_GATT_WriteCharacteristic(uint16_t conn_handle, uint16_t char_handle, 
                                  const uint8_t *data, uint16_t len);
int BLE_GATT_EnableNotification(uint16_t conn_handle, uint16_t desc_handle);
int BLE_GATT_DisableNotification(uint16_t conn_handle, uint16_t desc_handle);
```

#### 1.6 Application Executor (NEW)

**Files:** `App/BLE_Gateway/Inc/module_execute.h` + `App/BLE_Gateway/Src/module_execute.c`

**Purpose:** Simplify integration into main.c - only 2 functions needed

**API:**

```c
void module_ble_init(void);   // Call once during initialization
void module_ble_start(void);  // Call in main loop to process AT commands
```

**Implementation:**
- Initialize all modules in correct order
- Automatically register callbacks
- Process AT commands from circular buffer
- Single entry point - easy to maintain

---

### TIER 2: HIGH PRIORITY (Completed)

#### 2.1 BLE Event Dispatcher

**Files:** `App/BLE_Gateway/Inc/ble_event_handler.h` + `App/BLE_Gateway/Src/ble_event_handler.c`

**Purpose:** Centralize all HCI event callbacks

```c
// Event callback types
typedef void (*BLE_ConnectionCompleteCallback_t)(uint16_t conn_handle, const uint8_t *mac);
typedef void (*BLE_DisconnectionCompleteCallback_t)(uint16_t conn_handle, uint8_t reason);
typedef void (*BLE_GATTCNotificationCallback_t)(uint16_t conn_handle, uint16_t handle, 
                                                 const uint8_t *data, uint16_t len);

void BLE_EventHandler_Init(void);
void BLE_EventHandler_RegisterConnectionCallback(BLE_ConnectionCompleteCallback_t cb);
void BLE_EventHandler_RegisterDisconnectionCallback(BLE_DisconnectionCompleteCallback_t cb);
void BLE_EventHandler_RegisterNotificationCallback(BLE_GATTCNotificationCallback_t cb);
```

#### 2.2 Circular Buffer for UART RX

**Files:** `App/BLE_Gateway/Inc/circular_buffer.h` + `App/BLE_Gateway/Src/circular_buffer.c`

```c
typedef struct {
    uint8_t *buffer;
    uint16_t size;
    uint16_t head;
    uint16_t tail;
} CircularBuffer_t;

void CircularBuffer_Init(CircularBuffer_t *cb, uint8_t *buf, uint16_t size);
void CircularBuffer_Put(CircularBuffer_t *cb, uint8_t data);
int  CircularBuffer_Get(CircularBuffer_t *cb, uint8_t *data);
int  CircularBuffer_GetLine(CircularBuffer_t *cb, char *line, uint16_t max_len);
```

#### 2.3 Debug Trace Module (USB CDC Only!)

**Files:** `App/BLE_Gateway/Inc/debug_trace.h` + `App/BLE_Gateway/Src/debug_trace.c`

**CRITICAL:** Debug output ONLY via USB CDC (printf), not via UART!

```c
void DEBUG_PRINT(const char *fmt, ...);              // Uses printf() -> USB CDC
void DEBUG_PRINT_MAC(const uint8_t *mac);
void DEBUG_PRINT_HEX(const uint8_t *data, uint16_t len);
void DEBUG_PrintConnectionInfo(uint16_t conn_handle);
void DEBUG_PrintDeviceList(void);
```

---

### TIER 3: MEDIUM PRIORITY (Can Implement Later)

#### 3.1 Configuration and Persistent Storage

- Save favorite device list to EEPROM/Flash
- Configure scan parameters
- Configure connection timeout

#### 3.2 State Machine for Multi-Connection

- Handle state for 8 simultaneous connections
- Handle priority when scanning/connecting multiple devices

#### 3.3 Security and Encryption

- Pairing management
- CCCD (Client Characteristic Configuration Descriptor) handling
- Encryption enable/disable

---

## Implementation Workflow

### Phase 1: Foundation (Week 1) - COMPLETED

1. Created folder structure `App/BLE_Gateway/Inc` and `App/BLE_Gateway/Src`
2. Redirected printf() to USB CDC (modified _write() in main.c)
3. Created circular_buffer module
4. Created at_command module with basic parser (UART RX/TX only)
5. Created ble_device_manager module
6. Modified hci_le_advertising_report_event() - disabled auto-connect
7. Enabled LPUART1 interrupt in NVIC (CubeMX)
8. Increased HeapSize and StackSize (CubeMX)
9. Updated CMakeLists.txt to include App/BLE_Gateway sources

### Phase 2: Core BLE Functions (Week 2) - COMPLETED

1. Implemented BLE_Connection_CreateConnection() - GAP connect
2. Implemented BLE_Connection_TerminateConnection() - GAP disconnect
3. Modified event handlers for connection complete/disconnect
4. Tested AT+SCAN - Scan and print devices
5. Tested AT+CONNECT=XX:XX:XX:XX:XX:XX - Connect to device

### Phase 3: GATT Client Operations (Week 3) - COMPLETED

1. Implemented discovery functions
2. Implemented read/write characteristic functions
3. Implemented notification enable/disable
4. Tested AT+READ, AT+WRITE, AT+NOTIFY commands

### Phase 4: Testing and Optimization (Week 4) - COMPLETED

1. Multi-device stress test (8 devices)
2. AT command robustness testing
3. Memory profiling and optimization
4. Error handling and recovery

---

## AT Command Protocol

### Command Format: `AT+<CMD>=<PARAM1>,<PARAM2>...`

| Command | Format | Example | Response |
|---------|--------|---------|----------|
| **SCAN** | `AT+SCAN=<duration_ms>` | `AT+SCAN=5000` | `+SCAN_DEVICE:MAC,RSSI` or `OK` |
| **LIST** | `AT+LIST` | `AT+LIST` | `+DEV:idx,MAC,RSSI,CONN_HANDLE` or `OK` |
| **CONNECT** | `AT+CONNECT=<MAC>` | `AT+CONNECT=AA:BB:CC:DD:EE:FF` | `OK` / `+CONNECTING:idx` |
| **DISCONNECT** | `AT+DISCONNECT=<idx>` | `AT+DISCONNECT=0` | `OK` / `+DISCONNECTED:idx` |
| **READ** | `AT+READ=<idx>,<handle>` | `AT+READ=0,20` | `+READ:hex_data` or `ERROR` |
| **WRITE** | `AT+WRITE=<idx>,<handle>,<hex_data>` | `AT+WRITE=0,20,0102030405` | `OK` / `ERROR` |
| **NOTIFY** | `AT+NOTIFY=<idx>,<handle>,<enable>` | `AT+NOTIFY=0,21,1` | `OK` / `ERROR` |
| **DISC** | `AT+DISC=<idx>` | `AT+DISC=0` | `+DISC_SVC:handle,uuid` or `OK` |
| **INFO** | `AT+INFO=<idx>` | `AT+INFO=0` | Device information |

---

## Directory Structure

```
STM32_WB55_BLE_V2/
├── App/
│   └── BLE_Gateway/              [All custom application code]
│       ├── Inc/
│       │   ├── at_command.h              [AT parser for LPUART1]
│       │   ├── ble_device_manager.h      [Device list management]
│       │   ├── ble_connection.h          [Multi-connection state]
│       │   ├── ble_gatt_client.h         [GATT operations]
│       │   ├── ble_event_handler.h       [BLE event callbacks]
│       │   ├── circular_buffer.h         [UART RX buffer]
│       │   └── debug_trace.h             [USB CDC debug only]
│       └── Src/
│           ├── at_command.c
│           ├── ble_device_manager.c
│           ├── ble_connection.c
│           ├── ble_gatt_client.c
│           ├── ble_event_handler.c
│           ├── circular_buffer.c
│           └── debug_trace.c
├── Inc/                          [STM32CubeMX - DO NOT EDIT manually]
│   ├── app_ble.h
│   ├── p2p_client_app.h
│   ├── main.h
│   └── usbd_cdc_if.h
├── Src/                          [STM32CubeMX - Edit in USER CODE sections only]
│   ├── app_ble.c                 [Modified - integrate Gateway init]
│   ├── p2p_client_app.c          [Modified - scan callback to manager]
│   ├── main.c                    [Modified - USB CDC printf redirect]
│   ├── usbd_cdc_if.c             [Modified - CDC_Receive_FS callback]
│   └── stm32wbxx_it.c            [Modified - UART interrupt]
└── CMakeLists.txt                [Modified - add App/BLE_Gateway/**/*.c]
```

**Key Points:**
- All custom code isolated in `App/BLE_Gateway/`
- CubeMX can regenerate Inc/Src without destroying custom code
- Clear separation: LPUART1 for AT commands, USB CDC for debug printf
- Easy to add to CMakeLists: `file(GLOB_RECURSE GATEWAY_SOURCES "App/BLE_Gateway/Src/*.c")`

---

## API Usage Examples

### A. Scan Devices

```c
// 1. Enable scan
hci_le_set_scan_parameters(0x01, 0x0010, 0x0010, 0x01, 0x00);  // Active scan, 10ms interval
hci_le_set_scan_enable(0x01, 0x00);  // Enable, no duplicate filtering

// 2. Handle event in hci_le_advertising_report_event()
// Extract MAC, RSSI, add to device list
// Print: "+SCAN_DEVICE:XX:XX:XX:XX:XX:XX,RSSI:dBm"

// 3. Stop scan
hci_le_set_scan_enable(0x00, 0x00);  // Disable scan
```

### B. Connect to Device

```c
// AT+CONNECT=AA:BB:CC:DD:EE:FF
// 1. Find device by MAC
idx = BLE_DeviceManager_FindDevice(mac);

// 2. Initiate connection
hci_le_create_connection(
    scan_interval,
    scan_window,
    initiator_filter_policy,
    peer_addr_type,
    peer_addr,
    own_addr_type,
    conn_interval_min,
    conn_interval_max,
    conn_latency,
    supervision_timeout,
    min_ce_length,
    max_ce_length
);

// 3. Handle HCI_LE_CONNECTION_COMPLETE_EVENT
// Extract conn_handle, update device manager
```

### C. Discover Services

```c
// After connection established
aci_gattc_discover_all_services(conn_handle);

// Handle discovery events
// Store service: [service_handle, start_handle, end_handle, uuid]
```

### D. Read Characteristic

```c
// AT+READ=0,20
aci_gattc_read_char_value(conn_handle, char_handle);

// Handle read response event
// Print: "+READ:hex_data" or "ERROR"
```

---

## Error Handling

### Common Error Codes

- `0x01` - Unknown HCI Command
- `0x05` - Authentication Failure
- `0x0C` - Command Disallowed
- `0x11` - Unsupported Feature
- `0x1C` - Unknown Connection Identifier
- `0x1D` - Hardware Failure
- `0x3E` - Connection Terminated by Local Host
- `0x3F` - Connection Terminated by Remote Host

### Recovery Strategy

1. Log error code
2. Update device state
3. Clean up resources
4. Return error response to AT command
5. Allow retry

---

## Memory Considerations

### Current Allocation (INSUFFICIENT)

- HeapSize: 0x200 (512 bytes) - **TOO SMALL**
- StackSize: 0x400 (1KB) - **TOO SMALL**

### Recommended Settings

- HeapSize: 0x3000 (12KB) - BLE buffers + device list
- StackSize: 0x1000 (4KB) - Function call depth

### Memory Usage Per Device

- Device info: ~50 bytes
- Services: ~300 bytes
- Characteristics: ~500 bytes
- **Total per device: ~850 bytes**
- **For 8 devices: ~6.8KB**

---

## Testing Checklist

- [ ] UART interrupt working
- [ ] AT command parsing OK
- [ ] Scan displays devices
- [ ] Connect to single device
- [ ] Disconnect from device
- [ ] Discover services
- [ ] Read characteristic
- [ ] Write characteristic
- [ ] Enable notification
- [ ] Receive notification
- [ ] Multi-device connect (2+, 4+, 8)
- [ ] Connection timeout handling
- [ ] Device disconnection detection
- [ ] AT command error handling
- [ ] Memory leak check

---

## Detailed API Reference

### GAP Commands (Central Role)

```c
// Scan control (HCI LE)
hci_le_set_scan_parameters()
hci_le_set_scan_enable()
hci_le_set_extended_scan_parameters()
hci_le_set_extended_scan_enable()

// Connection control
hci_le_create_connection()
hci_le_create_connection_cancel()
hci_disconnect()

// GAP (higher level)
aci_gap_start_procedure()  // 0x00=Scan, 0x01=Connection, etc
aci_gap_terminate_proc()
```

### GATT Client Discovery (HCI vs ACI)

```c
// Low-level (HCI)
hci_le_gattc_read_by_type_request()
hci_le_gattc_read_by_group_type_request()
hci_le_gattc_find_info_request()

// High-level (ACI) - RECOMMENDED
aci_gattc_discover_all_services()
aci_gattc_discover_primary_services()
aci_gattc_discover_all_chars()
aci_gattc_discover_all_char_desc()
```

### GATT Client Read/Write

```c
// Read
aci_gattc_read_char_value(conn_handle, char_handle)
aci_gattc_read_long_char_value()

// Write
aci_gattc_write_char_value(conn_handle, char_handle, value, len)
aci_gattc_write_char_desc(conn_handle, handle, value)

// Notifications
hci_le_gattc_notification_event()  // Event callback
aci_gattc_write_char_desc()        // Enable CCCD
```

---

**Status:** Phase 1 Complete + Optimized  
**Ready for:** Production Testing
