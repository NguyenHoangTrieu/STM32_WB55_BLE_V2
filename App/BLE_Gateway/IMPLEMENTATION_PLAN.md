# STM32WB BLE Gateway - Thực hiện (Implementation Plan)

## Ngày tạo: 13/01/2025 | Cập nhật: 19/01/2026
**Mục tiêu**: Xây dựng Gateway trung tâm (Hub) quản lý nhiều thiết bị BLE thông qua lệnh AT
**Status**: ✅ Phase 1 Complete + Optimized

---

## 🔧 KIẾN TRÚC HỆ THỐNG

### Communication Channels:
- **LPUART1** (921600 baud): AT Command interface (RX/TX data only)
- **USB CDC**: Debug output via `printf()` - NO AT COMMANDS
- **BLE Stack**: Multi-device GATT Client operations

### Folder Structure:
```
STM32_WB55_BLE_V2/
├── App/
│   └── BLE_Gateway/          [NEW - All custom modules here]
│       ├── Inc/
│       │   ├── at_command.h
│       │   ├── ble_device_manager.h
│       │   ├── ble_connection.h
│       │   ├── ble_gatt_client.h       [OPTIMIZED - write/notify only]
│       │   ├── ble_event_handler.h
│       │   ├── debug_trace.h
│       │   └── module_execute.h        [NEW - Application entry point]
│       └── Src/
│           ├── at_command.c
│           ├── ble_device_manager.c
│           ├── ble_connection.c
│           ├── ble_gatt_client.c       [OPTIMIZED - removed unused stubs]
│           ├── ble_event_handler.c
│           ├── debug_trace.c
│           └── module_execute.c        [NEW - Init + main loop handler]
├── Inc/                       [STM32CubeMX generated]
│   ├── app_ble.h
│   ├── p2p_client_app.h
│   └── main.h
├── Src/                       [STM32CubeMX generated]
│   ├── app_ble.c              [MODIFIED - integrate Gateway modules]
│   ├── p2p_client_app.c       [MODIFIED - scan callback]
│   ├── main.c                 [MODIFIED - USB CDC printf]
│   └── usbd_cdc_if.c          [MODIFIED - CDC data callback]
└── CMakeLists.txt             [MODIFIED - add App/BLE_Gateway sources]
```

---

## I. PHÂN TÍCH API STM32_WPAN

### 1. **HCI Layer APIs** (ble_hci_le.h)
Các hàm điều khiển mức HCI:

#### A. Connection Management
- `hci_disconnect(conn_handle, reason)` - Ngắt kết nối
- `hci_read_remote_version_information(conn_handle)` - Đọc thông tin phiên bản thiết bị
- `hci_read_transmit_power_level(conn_handle, type, &power)` - Đọc mức công suất

#### B. Controller Configuration  
- `hci_reset()` - Reset BLE controller
- `hci_set_event_mask(event_mask)` - Cấu hình event cần nhận
- `hci_set_controller_to_host_flow_control()` - Điều khiển flow control

#### C. LE Scanning & Connection (hci_le_* functions)
**CRITICAL FUNCTIONS**:
- `hci_le_set_scan_parameters()` - Cấu hình scan (type, interval, window)
- `hci_le_set_scan_enable(enable, filter_dup)` - Bắt đầu/dừng scan
- `hci_le_create_connection()` - Khởi tạo kết nối tới device
- `hci_le_create_connection_cancel()` - Hủy quá trình kết nối
- `hci_le_set_default_phy()` - Cấu hình PHY (1M, 2M, Coded)
- `hci_le_set_default_periodic_advertising()` - Cấu hình periodic advertising

### 2. **GAP APIs** (ble_gap_aci.h)
Các hàm Generic Access Profile:

#### A. Discovery/Advertising (Peripheral role)
- `aci_gap_set_non_discoverable()` - Tắt quảng cáo
- `aci_gap_set_limited_discoverable()` - Chế độ discoverable giới hạn
- `aci_gap_set_discoverable()` - Chế độ discoverable tổng quát

#### B. Connection Management (Central role)
- `aci_gap_start_procedure(proc_code, ...)` - Bắt đầu procedure (scan, connect...)
- `aci_gap_terminate_proc(proc_code)` - Kết thúc procedure
- `aci_gap_create_connection()` - Tạo kết nối
- `aci_gap_terminate()` - Kết thúc kết nối

#### C. Device Information
- `aci_gap_get_bdaddr(addr_type, &addr)` - Lấy địa chỉ BLE
- `aci_gap_update_adv_data()` - Cập nhật advertising data
- `aci_gap_get_security_level()` - Lấy mức bảo mật

### 3. **GATT APIs** (ble_gatt_aci.h)

#### A. GATT Initialization
- `aci_gatt_init()` - **MUST CALL FIRST** - Khởi tạo GATT layer

#### B. Service Management (Server)
- `aci_gatt_add_service()` - Thêm service vào server
- `aci_gatt_add_char()` - Thêm characteristic vào service
- `aci_gatt_add_char_desc()` - Thêm descriptor
- `aci_gatt_del_char()` - Xóa characteristic
- `aci_gatt_del_service()` - Xóa service

#### C. Characteristic Read/Write (Server)
- `aci_gatt_update_char_value()` - Cập nhật giá trị characteristic
- `aci_gatt_set_event_mask()` - Cấu hình event notification

#### D. GATT Client Operations (Central role - CẬP CHÍ QUAN TRỌNG)
- `aci_gattc_read_char_value(conn_handle, char_handle)` - Đọc giá trị characteristic
- `aci_gattc_write_char_value(conn_handle, char_handle, value, len)` - Ghi giá trị
- `aci_gattc_write_char_desc(conn_handle, handle, value)` - Ghi descriptor (enable notify)
- `aci_gattc_discover_all_services(conn_handle)` - Discover tất cả services
- `aci_gattc_discover_primary_services(conn_handle, uuid_type, uuid)` - Discover service cụ thể
- `aci_gattc_discover_all_chars(conn_handle, start_handle, end_handle)` - Discover characteristics
- `aci_gattc_discover_all_char_desc(conn_handle, start_handle, end_handle)` - Discover descriptors

### 4. **HCI Events** (ble_events.h)

#### A. Connection Events
- `hci_le_connection_complete_event()` - Device kết nối thành công
- `hci_disconnection_complete_event()` - Device ngắt kết nối
- `hci_le_enhanced_connection_complete_event()` - Connection complete (enhanced)

#### B. Scanning Events
- `hci_le_advertising_report_event()` - **CRITICAL** - Nhận advertising packet khi scan
  - Trích xuất MAC address từ `report->address`
  - Lấy RSSI từ `report->rssi`
  - Lấy advertising data từ `report->data`

#### C. GATT Events
- `hci_le_gattc_notification_event()` - Nhận notification từ remote device
- `hci_le_gattc_read_response()` - Phản hồi read characteristic
- `hci_le_gattc_write_response()` - Phản hồi write characteristic
- `hci_le_gattc_disc_read_char_by_uuid_resp()` - Phản hồi discover services

#### D. Encryption Events
- `hci_encryption_change_event()` - Encryption bật/tắt
- `hci_encryption_key_refresh_complete_event()` - Key refresh hoàn tất

---

## II. CẤU TRÚC DỮ LIỆU HIỆN CÓ

### File hiện tại:
```
Inc/
  app_ble.h          - BLE application header
  p2p_client_app.h   - P2P Client template
  
Src/
  app_ble.c          - BLE initialization & callbacks
  p2p_client_app.c   - P2P Client state machine
  main.c             - Main loop
```

### Enums hiện có:
```c
APP_BLE_ConnStatus_t {
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
}

P2PC_APP_Opcode_Notification_evt_t {
  PEER_CONN_HANDLE_EVT,
  PEER_DISCON_HANDLE_EVT,
}
```

---

## III. THIẾU SÓT HIỆN TẠI & CẦN IMPLEMENT

### **TIER 1: CRITICAL (Phải làm ngay)**

#### 1.1 UART AT Command Parser Module
**File**: `App/BLE_Gateway/Inc/at_command.h` + `App/BLE_Gateway/Src/at_command.c`

**CRITICAL**: UART chỉ dùng cho AT command, KHÔNG dùng printf!

**API cần:**
```c
// AT Command Handler
void AT_Command_Init(void);
void AT_Command_Process(const char *cmd_line);
void AT_Command_UART_IRQHandler(void);

// Internal functions
int AT_SCAN_Handler(void);                          // AT+SCAN
int AT_CONNECT_Handler(const uint8_t *mac);        // AT+CONNECT=XX:XX:XX:XX:XX:XX
int AT_DISCONNECT_Handler(uint8_t conn_idx);       // AT+DISCONNECT=<idx>
int AT_LIST_Handler(void);                          // AT+LIST (list connected devices)
int AT_READ_Handler(uint8_t conn_idx, uint16_t handle);  // AT+READ=<idx>,<handle>
int AT_WRITE_Handler(uint8_t conn_idx, uint16_t handle, const uint8_t *data); // AT+WRITE=<idx>,<handle>,<data>
int AT_NOTIFY_Handler(uint8_t conn_idx, uint16_t handle, uint8_t enable); // AT+NOTIFY=<idx>,<handle>,<en>

// UART TX only for responses (NO printf here!)
void UART_SendATResponse(const char *fmt, ...);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
```

**Chức năng:**
- Parse chuỗi AT command từ LPUART1
- Validate syntax
- Trả về OK/ERROR qua UART TX
- Sử dụng circular buffer cho UART RX
- **KHÔNG** sử dụng printf (chỉ dùng HAL_UART_Transmit)

---

#### 1.2 BLE Device Manager Module
**File**: `App/BLE_Gateway/Inc/ble_device_manager.h` + `App/BLE_Gateway/Src/ble_device_manager.c`

**Cấu trúc dữ liệu:**
```c
typedef struct {
    uint8_t   mac_addr[6];              // MAC address
    uint16_t  conn_handle;              // Connection handle (-1 if not connected)
    uint8_t   is_connected;             // Connection status
    int8_t    rssi;                     // Last RSSI value
    uint8_t   service_count;            // Số service discovered
    // ... service & characteristic data
} BLE_Device_t;

// Manager structure
typedef struct {
    BLE_Device_t devices[8];            // Max 8 devices
    uint8_t      active_count;          // Số device hiện có
    uint8_t      scan_active;           // Scan đang chạy?
    uint16_t     scan_duration;         // Duration in ms
} BLE_DeviceManager_t;
```

**API cần:**
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

---

#### 1.3 Sửa Scan Callback (P2P Client)
**File**: `Src/p2p_client_app.c` - Function `hci_le_advertising_report_event()`

**Thay đổi:**
```c
// TRƯỚC (auto-connect):
// void hci_le_advertising_report_event(...) {
//     aci_gap_create_connection(...);  // Auto connect
// }

// SAU (in MAC, không auto-connect):
void hci_le_advertising_report_event(uint8_t Num_Reports,
                                     const hci_le_advertising_report_event_t *reports) {
    for(int i = 0; i < Num_Reports; i++) {
        const hci_le_advertising_report_event_t *report = &reports[i];
        
        // Extract MAC address
        uint8_t *mac = report->Address;
        int8_t rssi = report->RSSI;
        
        // Add to device manager
        BLE_DeviceManager_AddDevice(mac, rssi);
        
        // Print to UART
        printf("+SCAN_DEVICE:%02X:%02X:%02X:%02X:%02X:%02X,RSSI:%d\r\n",
               mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], rssi);
    }
}
```

---

#### 1.4 Multi-Device Connection Manager
**File**: `App/BLE_Gateway/Inc/ble_connection.h` + `App/BLE_Gateway/Src/ble_connection.c`

**Chức năng:**
```c
// Connection state tracking
typedef enum {
    CONN_STATE_IDLE,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_DISCOVERING,
    CONN_STATE_DISCONNECTING,
} BLE_ConnectionState_t;

// API
int  BLE_Connection_CreateConnection(const uint8_t *mac);
int  BLE_Connection_TerminateConnection(uint16_t conn_handle);
int  BLE_Connection_SetState(uint16_t conn_handle, BLE_ConnectionState_t state);
BLE_ConnectionState_t BLE_Connection_GetState(uint16_t conn_handle);
```

---

#### 1.5 GATT Client Write & Notify (OPTIMIZED)
**File**: `App/BLE_Gateway/Inc/ble_gatt_client.h` + `App/BLE_Gateway/Src/ble_gatt_client.c`

**Note**: Discovery và read operations đã được xóa vì chưa được implement và không được sử dụng.

**API (Simplified):**
```c
// Write/Notify only - actively used operations
void BLE_GATT_Init(void);
int BLE_GATT_WriteCharacteristic(uint16_t conn_handle, uint16_t char_handle, 
                                  const uint8_t *data, uint16_t len);
int BLE_GATT_EnableNotification(uint16_t conn_handle, uint16_t desc_handle);
int BLE_GATT_DisableNotification(uint16_t conn_handle, uint16_t desc_handle);
```

---

#### 1.6 Application Executor (NEW)
**File**: `App/BLE_Gateway/Inc/module_execute.h` + `App/BLE_Gateway/Src/module_execute.c`

**Mục đích:** Đơn giản hóa việc integrate vào main.c - chỉ cần 2 hàm

**API:**
```c
void module_ble_init(void);   // Call once during initialization
void module_ble_start(void);  // Call in main loop to process AT commands
```

**Implementation:**
- Khởi tạo tất cả modules theo đúng thứ tự
- Tự động đăng ký callbacks
- Xử lý AT commands từ circular buffer
- Single entry point - dễ maintain

---

### **TIER 2: HIGH PRIORITY (Đã hoàn thành)**

#### 2.1 BLE Event Dispatcher ✅
**File**: `App/BLE_Gateway/Inc/ble_event_handler.h` + `App/BLE_Gateway/Src/ble_event_handler.c`

**Mục đích:** Centralize tất cả HCI event callbacks

```c
// Event callback types
typedef void (*BLE_ConnectionCompleteCallback_t)(uint16_t conn_handle, const uint8_t *mac);
typedef void (*BLE_DisconnectionCompleteCallback_t)(uint16_t conn_handle, uint8_t reason);
typedef void (*BLE_GATTCNotificationCallback_t)(uint16_t conn_handle, uint16_t handle, 
                                                 const uint8_t *data, uint16_t len);
// ...

void BLE_EventHandler_Init(void);
void BLE_EventHandler_RegisterConnectionCallback(BLE_ConnectionCompleteCallback_t cb);
void BLE_EventHandler_RegisterDisconnectionCallback(BLE_DisconnectionCompleteCallback_t cb);
void BLE_EventHandler_RegisterNotificationCallback(BLE_GATTCNotificationCallback_t cb);
```

---

#### 2.2 Circular Buffer for UART RX ✅
**File**: `App/BLE_Gateway/Inc/circular_buffer.h` + `App/BLE_Gateway/Src/circular_buffer.c`

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

---

#### 2.3 Debug Trace Module (USB CDC Only!)
**File**: `App/BLE_Gateway/Inc/debug_trace.h` + `App/BLE_Gateway/Src/debug_trace.c`

**CRITICAL**: Debug chỉ qua USB CDC (printf), không qua UART!

```c
void DEBUG_PRINT(const char *fmt, ...);              // Uses printf() -> USB CDC
void DEBUG_PRINT_MAC(const uint8_t *mac);
void DEBUG_PRINT_HEX(const uint8_t *data, uint16_t len);
void DEBUG_PrintConnectionInfo(uint16_t conn_handle);
void DEBUG_PrintDeviceList(void);
```

---

### **TIER 3: MEDIUM PRIORITY (Có thể làm sau)**

#### 3.1 Configuration & Persistent Storage
- Lưu danh sách thiết bị yêu thích vào EEPROM/Flash
- Cấu hình scan parameters
- Cấu hình connection timeout

#### 3.2 State Machine for Multi-Connection
- Xử lý trạng thái cho 8 kết nối đồng thời
- Handle priority khi scan/connect nhiều device

#### 3.3 Security & Encryption
- Pairing management
- CCCD (Client Characteristic Configuration Descriptor) handling
- Encryption enable/disable

---

## IV. WORKFLOW IMPLEMENTATION

### **Phase 1: Foundation (Tuần 1)**
1. ✅ Tạo folder `App/BLE_Gateway/Inc` và `App/BLE_Gateway/Src`
2. ✅ Redirect printf() → USB CDC (sửa `_write()` trong main.c)
3. ✅ Tạo `circular_buffer` module
4. ✅ Tạo `at_command` module với parser cơ bản (chỉ UART RX/TX)
5. ✅ Tạo `ble_device_manager` module
6. ✅ Sửa `hci_le_advertising_report_event()` - tắt auto-connect
7. ✅ Bật LPUART1 interrupt trong NVIC (CubeMX)
8. ✅ Tăng HeapSize & StackSize (CubeMX)
9. ✅ Update CMakeLists.txt để include App/BLE_Gateway sources

### **Phase 2: Core BLE Functions (Tuần 2)**
1. ✅ Implement `BLE_Connection_CreateConnection()` - GAP connect
2. ✅ Implement `BLE_Connection_TerminateConnection()` - GAP disconnect
3. ✅ Sửa event handler cho connection complete/disconnect
4. ✅ Test AT+SCAN → Scan & print devices
5. ✅ Test AT+CONNECT=XX:XX:XX:XX:XX:XX → Connect to device

### **Phase 3: GATT Client Operations (Tuần 3)**
1. ✅ Implement discovery functions
2. ✅ Implement read/write characteristic functions
3. ✅ Implement notification enable/disable
4. ✅ Test AT+READ, AT+WRITE, AT+NOTIFY commands

### **Phase 4: Testing & Optimization (Tuần 4)**
1. ✅ Multi-device stress test (8 devices)
2. ✅ AT command robustness testing
3. ✅ Memory profiling & optimization
4. ✅ Error handling & recovery

---

## V. AT COMMAND PROTOCOL SPECIFICATION

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
| **INFO** | `AT+INFO=<idx>` | `AT+INFO=0` | Device info |

---

## VI. CẤU TRÚC THƯ MỤC SAU IMPLEMENT

```
STM32_WB55_BLE_V2/
├── App/
│   └── BLE_Gateway/              [NEW - All custom application code]
│       ├── Inc/
│       │   ├── at_command.h              [NEW - AT parser for LPUART1]
│       │   ├── ble_device_manager.h      [NEW - Device list management]
│       │   ├── ble_connection.h          [NEW - Multi-connection state]
│       │   ├── ble_gatt_client.h         [NEW - GATT operations]
│       │   ├── ble_event_handler.h       [NEW - BLE event callbacks]
│       │   ├── circular_buffer.h         [NEW - UART RX buffer]
│       │   └── debug_trace.h             [NEW - USB CDC debug only]
│       └── Src/
│           ├── at_command.c              [NEW]
│           ├── ble_device_manager.c      [NEW]
│           ├── ble_connection.c          [NEW]
│           ├── ble_gatt_client.c         [NEW]
│           ├── ble_event_handler.c       [NEW]
│           ├── circular_buffer.c         [NEW]
│           └── debug_trace.c             [NEW]
├── Inc/                          [STM32CubeMX - DO NOT EDIT manually]
│   ├── app_ble.h
│   ├── p2p_client_app.h
│   ├── main.h
│   └── usbd_cdc_if.h
├── Src/                          [STM32CubeMX - Edit in USER CODE sections only]
│   ├── app_ble.c                 [MODIFIED - integrate Gateway init]
│   ├── p2p_client_app.c          [MODIFIED - scan callback to manager]
│   ├── main.c                    [MODIFIED - USB CDC printf redirect]
│   ├── usbd_cdc_if.c             [MODIFIED - CDC_Receive_FS callback]
│   └── stm32wbxx_it.c            [MODIFIED - UART interrupt]
└── CMakeLists.txt                [MODIFIED - add App/BLE_Gateway/**/*.c]
```

**Key Points**:
- ✅ All custom code isolated in `App/BLE_Gateway/`
- ✅ CubeMX can regenerate Inc/Src without destroying custom code
- ✅ Clear separation: LPUART1=AT commands, USB CDC=debug printf
- ✅ Easy to add to CMakeLists: `file(GLOB_RECURSE GATEWAY_SOURCES "App/BLE_Gateway/Src/*.c")`

---

## VII. KEY API USAGE EXAMPLES

### A. Scan Device
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

### B. Connect Device
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

## VIII. ERROR HANDLING

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

## IX. MEMORY CONSIDERATIONS

### Current Allocation:
- HeapSize: 0x200 (512 bytes) ❌ **TOO SMALL**
- StackSize: 0x400 (1KB) ❌ **TOO SMALL**

### Recommended:
- HeapSize: 0x3000 (12KB) - BLE buffers + device list
- StackSize: 0x1000 (4KB) - Function calls depth

### Memory Usage Per Device:
- Device info: ~50 bytes
- Services: ~300 bytes
- Characteristics: ~500 bytes
- **Total per device: ~850 bytes**
- **For 8 devices: ~6.8KB**

---

## X. TESTING CHECKLIST

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

## XI. THAM CHIẾU API CHI TIẾT

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

### GATT Client R/W
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

**Cập nhật cuối**: 13/01/2025  
**Status**: ✅ Ready for Implementation (Phase 1)
