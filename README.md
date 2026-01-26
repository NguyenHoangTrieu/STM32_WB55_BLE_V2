# STM32 BLE Gateway - AT Command Interface

**Version**: 1.0  
**Last Updated**: January 20, 2026  
**Platform**: STM32WB55 (Cortex-M4 + Cortex-M0+)  
**Author**: Trieu Nguyen

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Features](#features)
4. [Communication Architecture](#communication-architecture)
5. [AT Command Reference](#at-command-reference)
6. [Quick Start Guide](#quick-start-guide)
7. [Integration Guide](#integration-guide)
8. [Example Workflows](#example-workflows)
9. [Module Architecture](#module-architecture)
10. [Troubleshooting](#troubleshooting)

---

## Project Overview

STM32 BLE Gateway is a complete BLE Central firmware for STM32WB55, enabling host MCU/PC to control BLE peripherals through a simple AT command protocol over UART.

### Key Capabilities

- **Multi-device scanning**: Scan and store up to 8 BLE devices simultaneously
- **Concurrent connections**: Support up to 8 concurrent device connections
- **GATT operations**: Write, Read, Notification/Indication support
- **Service discovery**: Automatic service and characteristic discovery
- **AT command interface**: Simple protocol, easy integration with any host

### Use Cases

- IoT Gateway controlling multiple BLE sensors/actuators
- BLE sniffer and debugging tool
- Bridge between BLE devices and Cloud/MQTT
- Testing and validation of BLE peripherals
- Educational BLE development platform

---

## Hardware Requirements

### Required Hardware

| Component | Specification |
|-----------|---------------|
| **MCU** | STM32WB55 (tested on NUCLEO-WB55) |
| **RAM** | Min 64KB (recommended 128KB) |
| **Flash** | Min 256KB (recommended 512KB) |
| **BLE Stack** | STM32WB Copro Wireless Binary v1.13+ |
| **Debug** | ST-Link V2/V3 or J-Link |

### Pinout Configuration

| Function | Pin | Configuration |
|----------|-----|---------------|
| **LPUART1 TX** | PA2 | AT Command output (921600 baud) |
| **LPUART1 RX** | PA3 | AT Command input (921600 baud) |
| **USB CDC** | USB | Debug console (printf redirect) |
| **LED1** | PB5 | Status indicator (optional) |

---

## Features

### Scanning and Discovery

- Active scanning with configurable duration
- Device name extraction from advertising data
- RSSI measurement and tracking
- Deduplication (report each device once per scan session)
- Support both Public and Random address types

### Connection Management

- Multi-device concurrent connections (max 8)
- Automatic connection parameter negotiation
- Connection state tracking
- Graceful disconnect handling
- Link loss detection

### GATT Client Operations

- Service discovery (primary services)
- Characteristic discovery
- Write with response
- Write without response
- Read characteristic value
- Enable/disable notifications
- Enable/disable indications

### Communication

- **UART**: 921600 baud, 8N1, no flow control
- **USB CDC**: Debug logging and system events
- Interrupt-driven RX with circular buffer
- AT command parsing with timeout protection

---

## Communication Architecture

### LPUART1 (921600 baud) - AT Command Interface

**Purpose**: Bidirectional AT command interface with host

**Configuration**:
- Baud rate: 921600 bps
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

**Protocol**:
- RX: Receive AT commands from host (terminated by `\r\n`)
- TX: Send responses and events to host
- Format: ASCII text commands and responses

**Important**: Only for AT commands, NOT for debug output!

### USB CDC - Debug Console

**Purpose**: Real-time debug logging and system monitoring

**Features**:
- `printf()` redirect via USB CDC
- System events logging
- BLE stack events
- Error messages and warnings

**Important**: Does NOT accept AT commands, output only!

---

## AT Command Reference

### Command Format

```
AT+<COMMAND>[=<param1>[,<param2>,...]]<CR><LF>
```

**Notes**:
- Commands are case-insensitive but UPPERCASE is recommended
- Parameters separated by commas
- Hex values can have optional `0x` prefix
- MAC addresses format: `AA:BB:CC:DD:EE:FF`
- Line terminator: `\r\n` (CR+LF)

### Response Format

```
[+<DATA>]
<STATUS>
```

**Status codes**:
- `OK` - Command executed successfully
- `ERROR` - Command failed
- `+ERROR:<reason>` - Error with specific reason

---

## Scanning and Discovery Commands

### `AT`

**Function**: Echo test - verify UART connection

**Parameters**: None

**Response**:
```
OK
```

**Example**:
```
Host → AT
     ← OK
```

---

### `AT+SCAN=<duration_ms>`

**Function**: Start BLE device scanning

**Parameters**:
- `duration_ms`: Scan duration in milliseconds (1-60000)

**Responses**:
- `OK` - Scan started successfully
- `+SCAN:<MAC>,<RSSI>,<name>` - Device discovered (once per scan session)
- `ERROR` - Failed to start scan

**Example**:
```
Host → AT+SCAN=5000
     ← OK
     ← +SCAN:AA:BB:CC:DD:EE:FF,-65,MyDevice
     ← +SCAN:11:22:33:44:55:66,-72,LightBulb
     ← +SCAN:22:33:44:55:66:77,-80,Unknown
```

**Notes**:
- Each device reported once per scan session even if advertising multiple times
- RSSI updated internally but not re-sent via UART within same scan
- Scan stops automatically after `duration_ms` or use `AT+STOP`
- Starting new scan resets reporting flags - devices will be reported again

---

### `AT+STOP`

**Function**: Stop active scanning

**Parameters**: None

**Response**:
```
OK
```

**Example**:
```
Host → AT+STOP
     ← OK
```

---

### `AT+LIST`

**Function**: List all discovered devices

**Parameters**: None

**Responses**:
- `+LIST:<count>` - Total device count
- `+DEV:<idx>,<MAC>,<RSSI>,<conn_handle>,<name>` - Each device info
- `OK` - Command complete

**Field descriptions**:
- `idx`: Device index (0-7) - used for other commands
- `MAC`: Device MAC address
- `RSSI`: Last measured signal strength (dBm)
- `conn_handle`: Connection handle (0xFFFF if not connected)
- `name`: Device name or "Unknown"

**Example**:
```
Host → AT+LIST
     ← +LIST:3
     ← +DEV:0,AA:BB:CC:DD:EE:FF,-65,0xFFFF,MyDevice
     ← +DEV:1,11:22:33:44:55:66,-72,0x0001,LightBulb
     ← +DEV:2,22:33:44:55:66:77,-80,0xFFFF,Unknown
     ← OK
```

---

### `AT+CLEAR`

**Function**: Clear device list

**Parameters**: None

**Response**:
```
OK
```

**Example**:
```
Host → AT+CLEAR
     ← OK
```

**Note**: Only clears discovered devices, does not affect active connections

---

## Connection Management Commands

### `AT+CONNECT=<MAC>`

**Function**: Connect to BLE device

**Parameters**:
- `MAC`: Device MAC address (format: AA:BB:CC:DD:EE:FF)

**Responses**:
- `OK` - Connection initiated
- `+CONNECTING` - Connection in progress
- `+CONNECTED:<idx>,<conn_handle>` - Connection established (async)
- `+CONN_ERROR:<status>` - Connection failed (async)
- `+ERROR:NOT_FOUND` - Device not in scan list

**Example**:
```
Host → AT+CONNECT=AA:BB:CC:DD:EE:FF
     ← OK
     ← +CONNECTING
     [... 200-500ms delay ...]
     ← +CONNECTED:0,0x0001
```

**Notes**:
- Device must be scanned first (`AT+SCAN`) before connecting
- Scan stops automatically when connection starts
- Connection timeout: ~2 seconds
- Supports concurrent connections up to 8 devices

---

### `AT+DISCONNECT=<idx>`

**Function**: Disconnect device

**Parameters**:
- `idx`: Device index (0-7) from `AT+LIST`

**Responses**:
- `OK` - Disconnect initiated
- `+DISCONNECTED:<conn_handle>` - Disconnected successfully (async)
- `+ERROR:NOT_CONNECTED` - Device not connected

**Example**:
```
Host → AT+DISCONNECT=0
     ← OK
     [... 50-100ms delay ...]
     ← +DISCONNECTED:0x0001
```

---

### `AT+INFO=<idx>`

**Function**: Get device detailed information

**Parameters**:
- `idx`: Device index (0-7)

**Responses**:
- `+INFO:<MAC>` - Device MAC address
- `OK` - Command complete
- `ERROR` - Invalid index

**Example**:
```
Host → AT+INFO=0
     ← +INFO:AA:BB:CC:DD:EE:FF
     ← OK
```

---

## GATT Operations Commands

### `AT+DISC=<idx>`

**Function**: Discover services and characteristics

**Parameters**:
- `idx`: Device index (0-7)

**Responses**:
- `OK` - Discovery started
- `+NAME:<device_name>` - Connected device name
- `+SERVICE:<conn_handle>,<service_handle>,<uuid>` - Service discovered (async, multiple)
- `+CHAR:<conn_handle>,<char_handle>,<uuid>` - Characteristic discovered (async, multiple)
- `+ERROR:NOT_CONNECTED` - Device not connected

**Example**:
```
Host → AT+DISC=0
     ← OK
     ← +NAME:Heart Rate Monitor
     ← +SERVICE:0x0001,0x0001,1800
     ← +SERVICE:0x0001,0x0005,180D
     ← +CHAR:0x0001,0x0002,2A00
     ← +CHAR:0x0001,0x0006,2A37
     ← +CHAR:0x0001,0x0008,2A38
```

**Notes**:
- Results arrive asynchronously via GATT events
- Service UUID and Char UUID in 16-bit short form
- Discovery may take 1-5 seconds depending on number of services

---

### `AT+WRITE=<idx>,<handle>,<data>`

**Function**: Write data to characteristic

**Parameters**:
- `idx`: Device index (0-7)
- `handle`: Characteristic value handle (hex, e.g., 0x000E or 000E)
- `data`: Hex data string (e.g., 01020304, max 64 bytes)

**Responses**:
- `OK` - Write completed
- `ERROR` - Write failed
- `+ERROR:NOT_CONNECTED` - Device not connected
- `+ERROR:INVALID_HEX` - Data format invalid

**Example**:
```
Host → AT+WRITE=0,0x000E,01020304
     ← OK
```

**Notes**:
- Uses Write Request (with response)
- Max data length: 64 bytes (128 hex characters)
- Data must be even-length hex string

---

### `AT+READ=<idx>,<handle>`

**Function**: Read characteristic value

**Parameters**:
- `idx`: Device index (0-7)
- `handle`: Characteristic value handle (hex)

**Responses**:
- `OK` - Read initiated
- `+READ:<conn_handle>,<handle>,<data_hex>` - Read result (async)
- `+ERROR:NOT_CONNECTED` - Device not connected

**Example**:
```
Host → AT+READ=0,0x000E
     ← OK
     [... 50-200ms delay ...]
     ← +READ:0x0001,0x000E,48656C6C6F
```

**Note**: Result arrives asynchronously via GATT read response event

---

### `AT+NOTIFY=<idx>,<desc_handle>,<enable>`

**Function**: Enable/disable notifications

**Parameters**:
- `idx`: Device index (0-7)
- `desc_handle`: CCCD descriptor handle (hex, usually char_handle + 1)
- `enable`: `1` = enable, `0` = disable

**Responses**:
- `OK` - CCCD written successfully
- `+NOTIFICATION:<conn_handle>,<handle>,<data_hex>` - Notification received (async, continuous)
- `+ERROR:NOT_CONNECTED` - Device not connected

**Example - Enable notifications**:
```
Host → AT+NOTIFY=0,0x000F,1
     ← OK
     [... when data arrives ...]
     ← +NOTIFICATION:0x0001,0x000E,5A
     ← +NOTIFICATION:0x0001,0x000E,5B
     ← +NOTIFICATION:0x0001,0x000E,5C
```

**Example - Disable notifications**:
```
Host → AT+NOTIFY=0,0x000F,0
     ← OK
```

**Notes**:
- CCCD handle typically = characteristic handle + 1
- Notifications arrive asynchronously when data available
- Can enable notifications for multiple characteristics

---

## Quick Start Guide

### Step 1: Hardware Setup

1. Flash STM32WB55 with BLE Copro Wireless Binary:
   ```bash
   STM32_Programmer_CLI -c port=SWD -fwupgrade stm32wb5x_BLE_Stack_full_fw.bin
   ```

2. Flash application firmware:
   ```bash
   make flash
   # or using STM32CubeIDE: Run > Debug As > STM32 MCU Debugging
   ```

3. Connect UART:
   - TX (PA2) → RX of host
   - RX (PA3) → TX of host
   - GND → GND
   - Baud rate: 921600, 8N1

4. Connect USB (optional for debug):
   - USB cable to ST-Link connector

### Step 2: Test Connection

```
Host → AT
     ← OK
```

### Step 3: Scan for Devices

```
Host → AT+SCAN=5000
     ← OK
     ← +SCAN:AA:BB:CC:DD:EE:FF,-65,MyDevice
     ← +SCAN:11:22:33:44:55:66,-72,LightBulb
```

### Step 4: Connect

```
Host → AT+CONNECT=AA:BB:CC:DD:EE:FF
     ← OK
     ← +CONNECTING
     ← +CONNECTED:0,0x0001
```

### Step 5: Discover Services

```
Host → AT+DISC=0
     ← OK
     ← +SERVICE:0x0001,0x0005,180D
     ← +CHAR:0x0001,0x0006,2A37
```

### Step 6: Enable Notifications

```
Host → AT+NOTIFY=0,0x0007,1
     ← OK
     ← +NOTIFICATION:0x0001,0x0006,5A
```

---

## Integration Guide

### For STM32CubeIDE Projects

**1. Add source files to project:**

```
Project/
├── App/
│   └── BLE_Gateway/
│       ├── Inc/
│       │   ├── at_command.h
│       │   ├── ble_connection.h
│       │   ├── ble_device_manager.h
│       │   ├── ble_gatt_client.h
│       │   ├── ble_event_handler.h
│       │   ├── debug_trace.h
│       │   └── module_execute.h
│       └── Src/
│           ├── at_command.c
│           ├── ble_connection.c
│           ├── ble_device_manager.c
│           ├── ble_gatt_client.c
│           ├── ble_event_handler.c
│           ├── debug_trace.c
│           └── module_execute.c
```

**2. Update CMakeLists.txt (if using CMake):**

```cmake
file(GLOB_RECURSE GATEWAY_SOURCES "App/BLE_Gateway/Src/*.c")
target_sources(${EXECUTABLE} PRIVATE ${GATEWAY_SOURCES})
target_include_directories(${EXECUTABLE} PRIVATE App/BLE_Gateway/Inc)
```

**3. Add to main.c:**

```c
/* USER CODE BEGIN Includes */
#include "module_execute.h"
/* USER CODE END Includes */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_LPUART1_UART_Init();

  /* USER CODE BEGIN 2 */
  module_ble_init();  // Initialize BLE Gateway
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    /* Sequencer handles everything */
    /* USER CODE END 3 */
  }
}
```

**4. Setup UART interrupt in stm32wbxx_it.c:**

```c
/* USER CODE BEGIN Includes */
#include "at_command.h"
/* USER CODE END Includes */

void LPUART1_IRQHandler(void)
{
  /* USER CODE BEGIN LPUART1_IRQn 0 */
  uint8_t byte;
  if (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_RXNE)) {
    byte = hlpuart1.Instance->RDR;
    AT_Command_ReceiveByte(byte);
  }
  /* USER CODE END LPUART1_IRQn 0 */

  HAL_UART_IRQHandler(&hlpuart1);

  /* USER CODE BEGIN LPUART1_IRQn 1 */
  /* USER CODE END LPUART1_IRQn 1 */
}
```

**5. Enable LPUART1 interrupt:**

```c
void MX_LPUART1_UART_Init(void)
{
  /* ... existing init code ... */

  /* Enable RXNE interrupt */
  __HAL_UART_ENABLE_IT(&hlpuart1, UART_IT_RXNE);
  HAL_NVIC_SetPriority(LPUART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(LPUART1_IRQn);
}
```

---

## Example Workflows

### Example 1: Heart Rate Monitor

```bash
# Scan for heart rate monitors
AT+SCAN=5000
OK
+SCAN:AA:BB:CC:DD:EE:FF,-65,HR Monitor

# Connect
AT+CONNECT=AA:BB:CC:DD:EE:FF
OK
+CONNECTING
+CONNECTED:0,0x0001

# Discover services
AT+DISC=0
OK
+SERVICE:0x0001,0x0005,180D
+CHAR:0x0001,0x0006,2A37
+CHAR:0x0001,0x0008,2A38

# Enable heart rate notifications (CCCD = 0x0007)
AT+NOTIFY=0,0x0007,1
OK
+NOTIFICATION:0x0001,0x0006,004A  # 74 BPM
+NOTIFICATION:0x0001,0x0006,004B  # 75 BPM
```

### Example 2: Smart Light Control

```bash
# Scan and connect to light bulb
AT+SCAN=3000
OK
+SCAN:11:22:33:44:55:66,-70,Smart Bulb

AT+CONNECT=11:22:33:44:55:66
OK
+CONNECTED:0,0x0001

# Discover characteristics
AT+DISC=0
OK
+CHAR:0x0001,0x000E,AAAA  # Custom control char

# Turn ON (write 0x01 to control char)
AT+WRITE=0,0x000E,01
OK

# Turn OFF (write 0x00)
AT+WRITE=0,0x000E,00
OK

# Set brightness to 50% (write 0x7F)
AT+WRITE=0,0x000E,7F
OK
```

### Example 3: Multi-Device Scenario

```bash
# Scan for multiple devices
AT+SCAN=5000
OK
+SCAN:AA:BB:CC:DD:EE:FF,-65,Sensor1
+SCAN:11:22:33:44:55:66,-70,Sensor2
+SCAN:22:33:44:55:66:77,-75,Actuator1

# Connect to all three
AT+CONNECT=AA:BB:CC:DD:EE:FF
OK
+CONNECTED:0,0x0001

AT+CONNECT=11:22:33:44:55:66
OK
+CONNECTED:1,0x0002

AT+CONNECT=22:33:44:55:66:77
OK
+CONNECTED:2,0x0003

# Enable notifications on both sensors
AT+NOTIFY=0,0x000F,1
OK
AT+NOTIFY=1,0x000F,1
OK

# Write command to actuator based on sensor data
+NOTIFICATION:0x0001,0x000E,FF  # Sensor1: temp high
AT+WRITE=2,0x000E,01            # Turn on fan
OK
```

---

## Module Architecture

### Software Modules

```
┌─────────────────────────────────────────────────┐
│              module_execute.c                    │
│         (Application Entry Point)                │
└─────────────┬────────────────────────────────────┘
              │
         ┌────┴────┐
         │         │
    ┌────▼───┐ ┌──▼──────────────┐
    │   AT   │ │  BLE Event      │
    │Command │ │   Handler       │
    └────┬───┘ └──┬──────────────┘
         │        │
    ┌────▼────────▼────┐
    │ BLE Connection    │
    │   Manager         │
    └────┬──────────────┘
         │
    ┌────▼──────────────┐
    │  Device Manager   │
    │  (Device List)    │
    └───────────────────┘
         │
    ┌────▼──────────────┐
    │  GATT Client      │
    │  Operations       │
    └───────────────────┘
```

### Module Descriptions

| Module | Responsibility | Size |
|--------|----------------|------|
| `module_execute.c` | Init and sequencer task registration | ~200 LOC |
| `at_command.c` | UART RX/TX, AT parsing, command dispatch | ~800 LOC |
| `ble_connection.c` | Scan, connect, disconnect, state management | ~300 LOC |
| `ble_device_manager.c` | Device list, MAC tracking, name storage | ~200 LOC |
| `ble_gatt_client.c` | GATT read/write/notify operations | ~250 LOC |
| `ble_event_handler.c` | BLE stack event routing | ~150 LOC |
| `debug_trace.c` | USB CDC debug helpers | ~100 LOC |

**Total code size**: ~2000 LOC, ~15KB Flash

---

## Troubleshooting

### Problem: `ERROR` response to all commands

**Causes**:
- BLE stack not initialized
- Invalid command syntax
- Device not in correct state

**Solutions**:
1. Check USB CDC debug output for errors
2. Verify `module_ble_init()` is called in `main()`
3. Test with `AT` command first
4. Check line terminator (`\r\n`)

---

### Problem: No `+SCAN` results

**Causes**:
- No BLE devices advertising nearby
- Wrong scan duration (too short)
- BLE stack issue

**Solutions**:
1. Increase scan duration: `AT+SCAN=10000`
2. Verify BLE devices are advertising (use phone app)
3. Check USB CDC for scan start/stop events
4. Reset board and retry

---

### Problem: `+ERROR:NOT_FOUND` when connecting

**Causes**:
- Device not scanned yet
- Device list cleared
- Wrong MAC address

**Solutions**:
1. Run `AT+SCAN` before connecting
2. Verify MAC address with `AT+LIST`
3. Don't run `AT+CLEAR` between scan and connect

---

### Problem: Connection timeout

**Causes**:
- Device out of range
- Device not connectable
- BLE stack busy

**Solutions**:
1. Move device closer (RSSI > -70dBm)
2. Verify device accepts connections
3. Wait 2-3 seconds between connection attempts
4. Check for `+CONN_ERROR` response

---

### Problem: Write/Read fails with `ERROR`

**Causes**:
- Device not connected
- Invalid handle
- Insufficient permissions

**Solutions**:
1. Run `AT+DISC` to find correct handles
2. Verify connection: `AT+LIST` (conn_handle != 0xFFFF)
3. Check characteristic properties (read/write/notify enabled?)

---

### Problem: No notifications received

**Causes**:
- CCCD not enabled
- Wrong descriptor handle
- Characteristic doesn't support notify

**Solutions**:
1. Enable CCCD: `AT+NOTIFY=<idx>,<cccd_handle>,1`
2. CCCD handle typically = char_handle + 1
3. Verify char properties support notification (from `AT+DISC`)

---

### Problem: Devices not reported in subsequent scans

**Causes**:
- Scan flag not reset between scan sessions
- Device manager not clearing report flags

**Solutions**:
1. Each `AT+SCAN` command automatically resets reporting flags
2. Devices will be reported again in new scan session
3. Use `AT+CLEAR` to completely clear device list if needed

---

## Configuration Notes

### IPCC Configuration in CubeMX

Remember to configure the IPCC (Inter-Processor Communication Controller) interrupt in CubeMX:
- Enable IPCC in CubeMX peripherals
- Configure IPCC interrupts
- Ensure IPCC is properly initialized before BLE stack operations

### FUS Firmware Startup

Important: The FUS (Firmware Upgrade Service) firmware does not automatically start. You must explicitly start it in your code before BLE operations:

```c
// Start FUS firmware before BLE initialization
// This is required for proper BLE stack operation
```

Ensure the FUS firmware is properly loaded and started before attempting any BLE operations.

---

**End of Documentation**