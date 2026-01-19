# BLE Gateway Application Modules

**Last Updated**: 19/01/2026  
**Status**: ✅ Phase 1 Complete + Optimized

## 📁 Cấu trúc thư mục

Thư mục này chứa tất cả các module tùy chỉnh cho BLE Gateway application.

```
BLE_Gateway/
├── Inc/           # Header files
└── Src/           # Implementation files
```

## 🔧 Communication Architecture

### LPUART1 (921600 baud) - AT Command Interface
- **Mục đích**: Nhận và trả lời AT commands
- **RX**: Parse AT commands từ host
- **TX**: Trả về responses (OK/ERROR/+DATA)
- **KHÔNG** sử dụng cho debug output!

### USB CDC - Debug Console
- **Mục đích**: Debug logging và system info
- **Redirect**: `printf()` → USB CDC via `_write()` in main.c
- **Output**: System events, BLE events, error logs
- **KHÔNG** nhận AT commands!

---

## 📟 AT Command Reference

### Basic Commands

#### `AT`
- **Chức năng**: Echo test - kiểm tra kết nối UART
- **Response**: `OK\r\n`
- **Ví dụ**:
  ```
  Host: AT\r\n
  Gateway: OK\r\n
  ```

---

### Scanning & Discovery

#### `AT+SCAN=<duration_ms>`
- **Chức năng**: Bắt đầu quét BLE devices
- **Tham số**: 
  - `duration_ms`: Thời gian quét (milliseconds)
- **Response**: 
  - `OK\r\n` - Bắt đầu quét thành công
  - `+SCAN:<MAC>,<RSSI>\r\n` - Mỗi device được tìm thấy
- **Ví dụ**:
  ```
  Host: AT+SCAN=5000\r\n
  Gateway: OK\r\n
  Gateway: +SCAN:AA:BB:CC:DD:EE:FF,-65\r\n
  Gateway: +SCAN:11:22:33:44:55:66,-72\r\n
  ```

#### `AT+LIST`
- **Chức năng**: Liệt kê tất cả devices đã phát hiện
- **Response**:
  - `+LIST:<count>\r\n` - Tổng số devices
  - `+DEV:<idx>,<MAC>,<RSSI>,<conn_handle>\r\n` - Thông tin từng device
  - `OK\r\n`
- **Ví dụ**:
  ```
  Host: AT+LIST\r\n
  Gateway: +LIST:2\r\n
  Gateway: +DEV:0,AA:BB:CC:DD:EE:FF,-65,0xFFFF\r\n
  Gateway: +DEV:1,11:22:33:44:55:66,-72,0x0001\r\n
  Gateway: OK\r\n
  ```

---

### Connection Management

#### `AT+CONNECT=<MAC>`
- **Chức năng**: Kết nối đến BLE device
- **Tham số**:
  - `MAC`: Địa chỉ MAC dạng AA:BB:CC:DD:EE:FF
- **Response**:
  - `OK\r\n` - Bắt đầu kết nối
  - `+CONNECTING\r\n` - Đang kết nối
  - `+CONNECTED:<idx>,<conn_handle>\r\n` - Kết nối thành công
  - `+CONN_ERROR:<status>\r\n` - Lỗi kết nối
- **Ví dụ**:
  ```
  Host: AT+CONNECT=AA:BB:CC:DD:EE:FF\r\n
  Gateway: OK\r\n
  Gateway: +CONNECTING\r\n
  Gateway: +CONNECTED:0,0x0001\r\n
  ```

#### `AT+DISCONNECT=<idx>`
- **Chức năng**: Ngắt kết nối device
- **Tham số**:
  - `idx`: Device index (0-7) từ AT+LIST
- **Response**:
  - `OK\r\n` - Bắt đầu ngắt kết nối
  - `+DISCONNECTED:<conn_handle>\r\n` - Đã ngắt kết nối
  - `ERROR\r\n` - Device không tồn tại hoặc không kết nối
- **Ví dụ**:
  ```
  Host: AT+DISCONNECT=0\r\n
  Gateway: OK\r\n
  Gateway: +DISCONNECTED:0x0001\r\n
  ```

---

### GATT Operations

#### `AT+WRITE=<idx>,<handle>,<data>`
- **Chức năng**: Ghi dữ liệu vào characteristic
- **Tham số**:
  - `idx`: Device index (0-7)
  - `handle`: Characteristic handle (hex, VD: 0x000E)
  - `data`: Dữ liệu hex (VD: 01020304)
- **Response**:
  - `OK\r\n` - Ghi thành công
  - `ERROR\r\n` - Device không kết nối hoặc handle không hợp lệ
- **Ví dụ**:
  ```
  Host: AT+WRITE=0,0x000E,01020304\r\n
  Gateway: OK\r\n
  ```
- **Note**: Hiện tại chỉ support write, chưa có read

#### `AT+NOTIFY=<idx>,<desc_handle>,<enable>`
- **Chức năng**: Bật/tắt notification cho characteristic
- **Tham số**:
  - `idx`: Device index (0-7)
  - `desc_handle`: CCCD descriptor handle (hex)
  - `enable`: 1 = enable, 0 = disable
- **Response**:
  - `OK\r\n` - Thành công
  - `ERROR\r\n` - Lỗi
- **Ví dụ**:
  ```
  Host: AT+NOTIFY=0,0x000F,1\r\n
  Gateway: OK\r\n
  Gateway: +NOTIFICATION:<conn_handle>,<handle>,<data_hex>\r\n (khi có data)
  ```

#### `AT+READ=<idx>,<handle>` ⚠️ NOT IMPLEMENTED
- **Chức năng**: Đọc giá trị characteristic
- **Status**: Đã bị xóa do không sử dụng
- **Alternative**: Sử dụng notification thay thế

#### `AT+DISC=<idx>` ⚠️ NOT IMPLEMENTED  
- **Chức năng**: Discover services và characteristics
- **Status**: Đã bị xóa do không sử dụng
- **Alternative**: Sử dụng handles cố định hoặc công cụ nRF Connect để discovery trước

---

### Device Information

#### `AT+INFO=<idx>`
- **Chức năng**: Lấy thông tin chi tiết device
- **Tham số**:
  - `idx`: Device index (0-7)
- **Response**:
  - `+INFO:<MAC>\r\n`
  - `OK\r\n`
- **Ví dụ**:
  ```
  Host: AT+INFO=0\r\n
  Gateway: +INFO:AA:BB:CC:DD:EE:FF\r\n
  Gateway: OK\r\n
  ```

---

## 📊 Response Format Summary

| Response | Meaning | When |
|----------|---------|------|
| `OK\r\n` | Command accepted | After valid command |
| `ERROR\r\n` | Command failed | Invalid syntax or device state |
| `+SCAN:<MAC>,<RSSI>\r\n` | Device found | During scan |
| `+CONNECTED:<idx>,<handle>\r\n` | Connection established | After AT+CONNECT |
| `+DISCONNECTED:<handle>\r\n` | Connection terminated | After disconnect or link loss |
| `+NOTIFICATION:<handle>,<data>\r\n` | Data received | When notification enabled |

## 📋 Modules cần implement

### Tier 1 - Critical (COMPLETE ✅)
1. ✅ **circular_buffer** - UART RX buffering
2. ✅ **at_command** - AT command parser và handler
3. ✅ **ble_device_manager** - Device list management
4. ✅ **ble_connection** - Multi-device connection state
5. ✅ **ble_gatt_client** - GATT client operations (optimized: write/notify only)
6. ✅ **module_execute** - Application entry point (NEW)

### Tier 2 - High Priority (COMPLETE ✅)
7. ✅ **ble_event_handler** - Centralized BLE event callbacks
8. ✅ **debug_trace** - Debug helper functions (USB CDC only)

### Tier 3 - Medium Priority
8. ⏳ **config_storage** - Persistent configuration
9. ⏳ **state_machine** - Multi-connection state management
10. ⏳ **security** - Pairing và encryption

## 🚀 Integration Steps

**SIMPLIFIED with module_execute:**

1. Add to CMakeLists.txt:
   ```cmake
   file(GLOB_RECURSE GATEWAY_SOURCES "App/BLE_Gateway/Src/*.c")
   target_sources(${EXECUTABLE} PRIVATE ${GATEWAY_SOURCES})
   target_include_directories(${EXECUTABLE} PRIVATE App/BLE_Gateway/Inc)
   ```

2. In main.c or app_entry.c:
   ```c
   #include "module_execute.h"
   
   // In initialization section (call once)
   module_ble_init();
   
   // In main loop
   while(1) {
       module_ble_start();
       // ... other tasks
   }
   ```

3. Setup UART interrupt in stm32wbxx_it.c:
   ```c
   #include "at_command.h"
   
   void LPUART1_IRQHandler(void) {
       uint8_t byte;
       if (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_RXNE)) {
           byte = hlpuart1.Instance->RDR;
           AT_Command_ReceiveByte(byte);
       }
       HAL_UART_IRQHandler(&hlpuart1);
   }
   ```

That's it! No manual initialization needed.

## ⚠️ Important Notes

- **NEVER** edit STM32CubeMX generated files trực tiếp
- Chỉ thêm code trong `/* USER CODE BEGIN */` sections
- Keep all custom code trong `App/BLE_Gateway/`
- UART = AT commands only, USB CDC = debug only
- Use `module_ble_init()` và `module_ble_start()` để integrate
- GATT client đã được tối ưu: chỉ write/notify operations
- Discovery và read operations đã bị xóa (không sử dụng)

## 🔧 Code Optimization (19/01/2026)

- Removed unused stub implementations from ble_gatt_client.c:
  - `BLE_GATT_DiscoverAllServices()`
  - `BLE_GATT_DiscoverCharacteristics()`
  - `BLE_GATT_DiscoverDescriptors()`
  - `BLE_GATT_ReadCharacteristic()`
  - `BLE_GATT_GetServiceCount()`
  - `BLE_GATT_GetService()`
- Added module_execute.c/h for simplified integration
- Kept only actively used GATT operations

## 📖 References

Xem `IMPLEMENTATION_PLAN.md` ở thư mục gốc project để biết chi tiết API và workflow.
