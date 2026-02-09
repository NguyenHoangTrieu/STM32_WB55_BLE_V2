# STM32 BLE Gateway - AT Command Interface

**Version**: 1.1  
**Last Updated**: February 9, 2026  
**Platform**: STM32WB55 (Cortex-M4 + Cortex-M0+)  
**Author**: Trieu Nguyen

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Features](#features)
4. [Communication Architecture](#communication-architecture)
5. [AT Command Reference](#at-command-reference)
   - [Scanning and Discovery Commands](#scanning-and-discovery-commands)
   - [Connection Management Commands](#connection-management-commands)
   - [GATT Operations Commands](#gatt-operations-commands)
   - [System and Lifecycle Commands](#system-and-lifecycle-commands)
   - [Configuration Commands](#configuration-commands)
   - [Mode Commands](#mode-commands)
   - [Status and Diagnostics Commands](#status-and-diagnostics-commands)
   - [Power Management Commands](#power-management-commands)
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

## System and Lifecycle Commands

### `AT+RESET`

**Function**: Software reset - restart the module

**Parameters**: None

**Response**:
```
OK
```

**Example**:
```
Host → AT+RESET
     ← OK
     [... module restarts ...]
```

**Notes**:
- Module will restart after 100ms delay
- All connections will be terminated
- Unsaved configuration changes will be lost

---

### `AT+HWRESET`

**Function**: Hardware reset - reset via hardware mechanism

**Parameters**: None

**Responses**:
- `OK` - Hardware reset initiated
- `+ERROR:NOT_SUPPORTED` - Hardware reset not available

**Example**:
```
Host → AT+HWRESET
     ← OK
```

**Note**: Availability depends on hardware configuration

---

### `AT+FACTORY`

**Function**: Factory reset - restore default settings and restart

**Parameters**: None

**Response**:
```
OK
```

**Example**:
```
Host → AT+FACTORY
     ← OK
     [... module resets to factory defaults and restarts ...]
```

**Notes**:
- All saved configuration will be erased
- Device list will be cleared
- Module restarts automatically after reset

---

## Configuration Commands

### `AT+GETINFO`

**Function**: Get module information (firmware version, BLE stack, BD address, uptime)

**Parameters**: None

**Responses**:
- `+FW:<version>` - Firmware version
- `+BLE:<version>` - BLE stack version
- `+BDADDR:<mac>` - Bluetooth device address
- `+UPTIME:<ms> ms` - Module uptime in milliseconds
- `OK` - Command complete

**Example**:
```
Host → AT+GETINFO
     ← +FW:1.0.0
     ← +BLE:STM32WB v1.13.0
     ← +BDADDR:AA:BB:CC:DD:EE:FF
     ← +UPTIME:123456 ms
     ← OK
```

---

### `AT+NAME=<name>`

**Function**: Set device name

**Parameters**:
- `name`: Device name string (max length depends on implementation)

**Responses**:
- `OK` - Name set successfully
- `ERROR` - Invalid name or operation failed

**Example**:
```
Host → AT+NAME=MyGateway
     ← OK
```

**Notes**:
- Use `AT+SAVE` to persist the name across reboots
- Name change takes effect immediately

---

### `AT+COMM=<baud>,<parity>,<stop>`

**Function**: Set UART communication parameters

**Parameters**:
- `baud`: Baud rate (e.g., 9600, 115200, 921600)
- `parity`: Parity setting
  - `0` = None
  - `1` = Even
  - `2` = Odd
- `stop`: Stop bits (1 or 2)

**Responses**:
- `OK` - Settings applied
- `ERROR` - Invalid parameters

**Example**:
```
Host → AT+COMM=115200,0,1
     ← OK
     [... UART reconfigures to new settings ...]
```

**Notes**:
- Settings apply immediately after OK response
- Host must switch to new settings to continue communication
- Use `AT+SAVE` to persist settings across reboots
- Data bits are fixed at 8

---

### `AT+RF=<tx_power>,<scan_interval>,<scan_window>`

**Function**: Set RF parameters (TX power and scan timing)

**Parameters**:
- `tx_power`: TX power in dBm (range depends on hardware, typically -20 to +6)
- `scan_interval`: Scan interval in 0.625ms units (e.g., 160 = 100ms)
- `scan_window`: Scan window in 0.625ms units (must be ≤ scan_interval)

**Responses**:
- `OK` - RF parameters set successfully
- `ERROR` - Invalid parameters

**Example**:
```
Host → AT+RF=0,160,80
     ← OK
```

**Notes**:
- TX power: 0 dBm (typical)
- Scan interval: 160 × 0.625ms = 100ms
- Scan window: 80 × 0.625ms = 50ms
- Settings apply immediately
- Use `AT+SAVE` to persist across reboots

---

### `AT+SAVE`

**Function**: Save current configuration to non-volatile memory (NVM)

**Parameters**: None

**Responses**:
- `OK` - Configuration saved successfully
- `ERROR` - Save operation failed

**Example**:
```
Host → AT+SAVE
     ← OK
```

**Notes**:
- Saves all configuration parameters (name, UART, RF settings)
- Configuration persists across power cycles and resets
- Does NOT save device list or connection states

---

## Mode Commands

### `AT+CMDMODE`

**Function**: Enter command mode (exit data mode if active)

**Parameters**: None

**Responses**:
- `OK` - Entered command mode
- `ERROR` - Failed to enter command mode

**Example**:
```
Host → AT+CMDMODE
     ← OK
```

**Notes**:
- In command mode, all UART data is interpreted as AT commands
- Use this to exit data mode and return to AT command processing

---

### `AT+DATAMODE=<dev_idx>,<char_handle>`

**Function**: Enter data mode - transparent UART to GATT characteristic bridge

**Parameters**:
- `dev_idx`: Device index (0-7)
- `char_handle`: Characteristic handle to write to

**Responses**:
- `OK` - Entered data mode
- `+ERROR:NOT_CONNECTED` - Device not connected

**Example**:
```
Host → AT+DATAMODE=0,0x000E
     ← OK
     [... all subsequent UART data is written to characteristic 0x000E ...]
```

**Notes**:
- In data mode, all UART RX data is written to the specified characteristic
- All notifications from the characteristic are sent to UART TX
- Exit data mode with escape sequence or `AT+CMDMODE`
- Device must be connected before entering data mode

---

## Status and Diagnostics Commands

### `AT+STATUS[=<dev_idx>]`

**Function**: Get connection status for device(s)

**Parameters**:
- `dev_idx`: (Optional) Device index (0-7). If omitted, reports all devices

**Responses**:
- Without parameter (all devices):
  - `+STATUS:<count> devices` - Total device count
  - `+DEV:<idx>,<status>,<conn_handle>` - Status for each device
- With parameter (specific device):
  - `+STATUS:<status>,<conn_handle>,RSSI=<rssi>` - Device status
- `OK` - Command complete
- `ERROR` - Invalid device index

**Field descriptions**:
- `status`: `CONNECTED` or `DISCONNECTED`
- `conn_handle`: Connection handle (hex)
- `rssi`: Signal strength in dBm

**Example - All devices**:
```
Host → AT+STATUS
     ← +STATUS:3 devices
     ← +DEV:0,CONNECTED,0x0001
     ← +DEV:1,DISCONNECTED,0xFFFF
     ← +DEV:2,CONNECTED,0x0002
     ← OK
```

**Example - Specific device**:
```
Host → AT+STATUS=0
     ← +STATUS:CONNECTED,0x0001,RSSI=-65
     ← OK
```

---

### `AT+DIAG=<dev_idx>`

**Function**: Get diagnostic information for a device

**Parameters**:
- `dev_idx`: Device index (0-7)

**Responses**:
- `+DIAG:RSSI=<rssi> dBm` - Signal strength
- `+DIAG:CONN_HANDLE=<handle>` - Connection handle (if connected)
- `+DIAG:STATUS=<status>` - Connection status
- `+DIAG:TX_POWER=<power> dBm` - Current TX power setting
- `OK` - Command complete
- `ERROR` - Invalid device index

**Example**:
```
Host → AT+DIAG=0
     ← +DIAG:RSSI=-65 dBm
     ← +DIAG:CONN_HANDLE=0x0001
     ← +DIAG:STATUS=CONNECTED
     ← +DIAG:TX_POWER=0 dBm
     ← OK
```

**Notes**:
- Useful for debugging connection issues
- RSSI value updated from last received packet
- TX power reflects current RF configuration

---

## Power Management Commands

### `AT+SLEEP=<mode>,<wake_mask>,<timeout_ms>`

**Function**: Enter low-power sleep mode

**Parameters**:
- `mode`: Sleep mode
  - `1` = Sleep mode (CPU stopped, peripherals active)
  - `2` = Stop0 mode (lower power, fast wake)
  - `3` = Stop1 mode (very low power)
  - `4` = Stop2 mode (ultra-low power)
- `wake_mask`: Wake source mask (bitfield)
  - `0x01` = UART wake
  - `0x02` = GPIO wake
  - `0x04` = Timer wake
  - (Combine with OR: e.g., `0x03` = UART or GPIO)
- `timeout_ms`: Auto-wake timeout in milliseconds (0 = no timeout)

**Responses**:
- `OK` - Entering sleep mode
- `+WAKE` - Module has woken up (sent after wake event)
- `+ERROR:INVALID_MODE` - Invalid sleep mode

**Example - Sleep with UART wake**:
```
Host → AT+SLEEP=1,0x01,0
     ← OK
     [... module enters sleep ...]
     [... UART activity wakes module ...]
     ← +WAKE
```

**Example - Sleep with timeout**:
```
Host → AT+SLEEP=2,0x01,5000
     ← OK
     [... module sleeps for 5 seconds ...]
     ← +WAKE
```

**Notes**:
- Module sends `OK` before entering sleep
- `+WAKE` event sent after waking up
- All active connections may be lost in deep sleep modes
- UART transmission completes before sleep entry

---

### `AT+WAKE`

**Function**: Manual wake command (no-op if already awake)

**Parameters**: None

**Response**:
```
OK
```

**Example**:
```
Host → AT+WAKE
     ← OK
```

**Note**: Always succeeds when module is in run mode

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