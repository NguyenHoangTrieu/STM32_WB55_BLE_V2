# BLE Gateway Application Modules

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

## 📋 Modules cần implement

### Tier 1 - Critical (MUST HAVE)
1. ✅ **circular_buffer** - UART RX buffering
2. ✅ **at_command** - AT command parser và handler
3. ✅ **ble_device_manager** - Device list management
4. ✅ **ble_connection** - Multi-device connection state
5. ✅ **ble_gatt_client** - GATT client operations

### Tier 2 - High Priority
6. ✅ **ble_event_handler** - Centralized BLE event callbacks
7. ✅ **debug_trace** - Debug helper functions (USB CDC only)

### Tier 3 - Medium Priority
8. ⏳ **config_storage** - Persistent configuration
9. ⏳ **state_machine** - Multi-connection state management
10. ⏳ **security** - Pairing và encryption

## 🚀 Integration Steps

1. Tạo module files trong Inc/ và Src/
2. Add to CMakeLists.txt:
   ```cmake
   file(GLOB_RECURSE GATEWAY_SOURCES "App/BLE_Gateway/Src/*.c")
   target_sources(${EXECUTABLE} PRIVATE ${GATEWAY_SOURCES})
   target_include_directories(${EXECUTABLE} PRIVATE App/BLE_Gateway/Inc)
   ```
3. Include headers trong main.c hoặc app_ble.c
4. Initialize modules in MX_APPE_Init()
5. Process trong main loop

## ⚠️ Important Notes

- **NEVER** edit STM32CubeMX generated files trực tiếp
- Chỉ thêm code trong `/* USER CODE BEGIN */` sections
- Keep all custom code trong `App/BLE_Gateway/`
- UART = AT commands only, USB CDC = debug only
- Test từng module độc lập trước khi integrate

## 📖 References

Xem `IMPLEMENTATION_PLAN.md` ở thư mục gốc project để biết chi tiết API và workflow.
