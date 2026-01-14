# STM32WB55 BLE Gateway - Code Review Chi Tiết

## Thông Tin Chung
- **Dự án**: STM32WB55 BLE Gateway
- **MCU**: STM32WB55 (Dual Core: CPU1 - Cortex-M4, CPU2 - Cortex-M0+)
- **FUS (Firmware Upgrade Service)**: v2.2.0
- **BLE Stack**: v1.24.0.3
- **Tệp chính**: `Src/main.c`

---

## 1. BIẾN TOÀN CỤC (Global Variables)

### 1.1 `static uint32_t delay = 250;`
- **Chức năng**: Lưu trữ thời gian trễ giữa các lần toggle LED (tính bằng milliseconds)
- **Giá trị mặc định**: 250ms
- **Cách dùng**: Được thay đổi bởi callback nút bấm để điều khiển tốc độ nhấp nháy LED
- **Phạm vi**: Static - chỉ nhìn thấy trong file main.c

### 1.2 `IPCC_HandleTypeDef hipcc;`
- **Chức năng**: Handle (bộ xử lý) cho IPCC (Inter-Processor Communication Controller)
- **Ý nghĩa**: Dùng để giao tiếp giữa hai CPU (CPU1 và CPU2)
- **Loại**: IPCC_HandleTypeDef - struct HAL chứa cấu hình và trạng thái IPCC
- **Khi nào sử dụng**: Cần thiết để giao tiếp với BLE Stack chạy trên CPU2

### 1.3 `UART_HandleTypeDef hlpuart1;`
- **Chức năng**: Handle cho LPUART1 (Low Power UART)
- **Ý nghĩa**: UART tốc độ thấp dùng để gửi/nhận dữ liệu debug qua serial port
- **Kết nối**: Được dùng bởi hàm `printf()` thông qua `_write()`

### 1.4 `RNG_HandleTypeDef hrng;`
- **Chức năng**: Handle cho RNG (Random Number Generator)
- **Ý nghĩa**: Cung cấp số ngẫu nhiên cho BLE Stack (dùng trong mã hóa, advertisements)

### 1.5 `RTC_HandleTypeDef hrtc;`
- **Chức năng**: Handle cho RTC (Real Time Clock)
- **Ý nghĩa**: Quản lý thời gian thực, có thể dùng cho timestamping hoặc wake-up events

---

## 2. HÀM PRINTF TÙYCHỈNH

### 2.1 `int _write(int file, char *ptr, int len)`

**Vị trí**: Dòng 70-74

```c
int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&hlpuart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}
```

**Chức năng**:
- Ghi đè hàm `_write()` chuẩn của newlib (C library)
- Cho phép `printf()` gửi dữ liệu qua UART thay vì stdout mặc định

**Cách hoạt động**:
1. Nhận con trỏ tới chuỗi ký tự (`ptr`)
2. Nhận độ dài chuỗi (`len`)
3. Dùng `HAL_UART_Transmit()` để gửi dữ liệu qua LPUART1
4. `HAL_MAX_DELAY` = chờ vô hạn cho đến khi gửi xong
5. Trả về số ký tự đã gửi

**API sử dụng**:
- `HAL_UART_Transmit()`: Gửi dữ liệu UART theo chế độ blocking (chặn chương trình)

---

## 3. HÀM MAIN() - ĐIỂM VÀO CỦA CHƯƠNG TRÌNH

### 3.1 Cấu trúc tổng quát:
```
HAL_Init()
  ↓
MX_APPE_Config() (BLE - sơ cấp)
  ↓
SystemClock_Config()
  ↓
PeriphCommonClock_Config()
  ↓
MX_IPCC_Init()
  ↓
Initialization (GPIO, UART, RNG, RTC, RF)
  ↓
C2BOOT Force Enable (Khẩn cấp)
  ↓
MX_APPE_Init() (BLE - chính)
  ↓
LED, Button Init
  ↓
WHILE(1) LOOP
```

### 3.2 Giai đoạn khởi tạo chi tiết:

#### a. `HAL_Init()` - Dòng 98
- **API**: `HAL_Init()` từ HAL Driver
- **Chức năng**: 
  - Reset tất cả peripheral
  - Khởi tạo Flash interface
  - Cấu hình Systick (để `HAL_Delay()` hoạt động)

#### b. `MX_APPE_Config()` - Dòng 100
- **Chức năng**: Cấu hình sơ cấp cho STM32_WPAN (BLE)
- **Tại sao sớm**: HSE (High Speed External oscillator) phải được tune trước cấu hình xung hệ thống

#### c. `SystemClock_Config()` - Dòng 106
- **Chức năng**: Cấu hình xung hệ thống và các oscillator
- **Chi tiết**: Xem mục 4

#### d. `PeriphCommonClock_Config()` - Dòng 109
- **Chức năng**: Cấu hình xung cho RF (SMPS - Switched Mode Power Supply, RF Wake-up)

#### e. `MX_IPCC_Init()` - Dòng 112
- **Chức năng**: Khởi tạo IPCC để giao tiếp CPU1 ↔ CPU2

#### f. Khởi tạo Peripheral - Dòng 117-121
```c
MX_GPIO_Init();      // Cấu hình GPIO (LED, Button)
MX_LPUART1_UART_Init();  // Khởi tạo UART debug
MX_RNG_Init();        // Khởi tạo Random Number Generator
MX_RTC_Init();        // Khởi tạo Real Time Clock
MX_RF_Init();         // Khởi tạo RF (để trống)
```

#### g. **C2BOOT Force Enable** - Dòng 123-135
**QUAN TRỌNG - CRITICAL CODE**:
```c
printf("Force enabling CPU2 boot...\r\n");

// Kiểm tra bit C2BOOT trong PWR_CR4
if((PWR->CR4 & PWR_CR4_C2BOOT) == 0) {
    printf("C2BOOT=0, setting now...\r\n");
    PWR->CR4 |= PWR_CR4_C2BOOT;  // Set bit C2BOOT
    HAL_Delay(10);
}

printf("PWR_CR4: 0x%08lX\r\n", PWR->CR4);
printf("C2BOOT Status: %s\r\n", (PWR->CR4 & PWR_CR4_C2BOOT) ? "SET" : "CLEAR");
```

**Ý nghĩa**:
- PWR_CR4 là thanh ghi Power Control Register 4
- C2BOOT bit cho phép CPU2 (Cortex-M0+) khởi động
- Nếu Option Bytes vô hiệu hóa C2BOOT, BLE Stack sẽ không chạy
- Đây là workaround cho những tình huống Option Bytes bị lock

**API sử dụng**:
- `PWR->CR4` - Truy cập trực tiếp thanh ghi PWR (nguy hiểm nhưng hiệu quả)
- `PWR_CR4_C2BOOT` - Macro định nghĩa vị trí bit

#### h. `MX_APPE_Init()` - Dòng 138
- **Chức náng**: Khởi tạo chính BLE Stack
- **Ghi chú**: Phải sau C2BOOT enable

#### i. Board Support Package (BSP) Init - Dòng 139-145
```c
BSP_LED_Init(LED_BLUE);    // Khởi tạo LED xanh
BSP_LED_Init(LED_GREEN);   // Khởi tạo LED xanh lục
BSP_LED_Init(LED_RED);     // Khởi tạo LED đỏ

BSP_PB_Init(BUTTON_SW1, BUTTON_MODE_EXTI);  // Nút 1 - Interrupt mode
BSP_PB_Init(BUTTON_SW2, BUTTON_MODE_EXTI);  // Nút 2 - Interrupt mode
BSP_PB_Init(BUTTON_SW3, BUTTON_MODE_EXTI);  // Nút 3 - Interrupt mode
```

**API sử dụng**:
- `BSP_LED_Init()`: Khởi tạo LED
- `BSP_PB_Init()`: Khởi tạo push-button với External Interrupt

#### j. Bật tất cả LED lúc đầu - Dòng 152-154
```c
BSP_LED_On(LED_BLUE);
BSP_LED_On(LED_GREEN);
BSP_LED_On(LED_RED);
```

### 3.3 VÒNG LẶP CHÍNH (Infinite Loop) - Dòng 157-176

```c
while (1) {
    BSP_LED_Toggle(LED_BLUE);
    HAL_Delay(delay);
    
    BSP_LED_Toggle(LED_GREEN);
    HAL_Delay(delay);
    
    BSP_LED_Toggle(LED_RED);
    HAL_Delay(delay);
    
    MX_APPE_Process();  // Xử lý BLE events
}
```

**Chức năng**:
- Nhấp nháy 3 LED theo thứ tự với khoảng trễ `delay`
- Gọi `MX_APPE_Process()` để xử lý các sự kiện BLE từ CPU2

**Cách hoạt động**:
1. Toggle LED BLUE → chờ `delay` ms → LED tắt
2. Toggle LED GREEN → chờ `delay` ms → LED tắt
3. Toggle LED RED → chờ `delay` ms → LED tắt
4. Xử lý BLE events
5. Lặp lại

**API sử dụng**:
- `BSP_LED_Toggle()`: Bật/tắt LED
- `HAL_Delay()`: Trì hoãn (blocking)

---

## 4. SYSTEM CLOCK CONFIGURATION

### 4.1 Hàm `SystemClock_Config()` - Dòng 178-233

**Mục đích**: Cấu hình xung hệ thống để đạt hiệu suất tối ưu

**Các bước**:

#### a. Cấu hình LSE Drive - Dòng 184-185
```c
HAL_PWR_EnableBkUpAccess();
__HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMHIGH);
```
- Mở khóa Backup Domain
- Cấu hình LSE (Low Speed External 32.768 kHz) với drive cao

#### b. Cấu hình điện áp - Dòng 189
```c
__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
```
- Scale 1 = Điện áp cao nhất = Hiệu suất cao nhất

#### c. Cấu hình Oscillators - Dòng 194-214
```c
RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI1 |
                                    RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE |
                                    RCC_OSCILLATORTYPE_MSI;
```

**Các oscillator**:
- **HSE** (High Speed External) - 32 MHz - BLE cần
- **HSI** (High Speed Internal) - ~16 MHz - Fallback
- **MSI** (Multi Speed Internal) - ~4 MHz - Điều chỉnh được
- **LSE** (Low Speed External) - 32.768 kHz - RTC, Low power
- **LSI** (Low Speed Internal) - ~32 kHz - Fallback for RTC

#### d. Cấu hình PLL - Dòng 206-211
```c
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;  // Từ MSI
RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;          // Divisor = 1
RCC_OscInitStruct.PLL.PLLN = 32;                      // Multiplier = 32
RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;          // Output P = /2
RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;          // Output R = /2
RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;          // Output Q = /2
```

**Tính toán**:
- MSI ≈ 4 MHz
- PLL_OUT = (MSI * 32) / 1 / 2 = (4 * 32) / 2 = 64 MHz
- SYSCLK = 64 MHz

#### e. Cấu hình Clock Dividers - Dòng 221-227
```c
RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;  // Từ PLL
RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;        // HCLK1 = 64 MHz
RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;         // PCLK1 = 64 MHz
RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;         // PCLK2 = 64 MHz
RCC_ClkInitStruct.AHBCLK2Divider = RCC_SYSCLK_DIV2;       // HCLK2 = 32 MHz (M0+)
RCC_ClkInitStruct.AHBCLK4Divider = RCC_SYSCLK_DIV1;       // HCLK4 = 64 MHz (AHB4)
```

**Ý nghĩa**:
- HCLK1: Bus chính (CPU1)
- HCLK2: Bus cho CPU2 (chậm hơn để tiết kiệm điện)
- HCLK4: Bus ngoại vi AHB4
- PCLK1, PCLK2: Bus APB cho peripheral

---

## 5. PERIPHERAL COMMON CLOCK CONFIGURATION

### 5.1 Hàm `PeriphCommonClock_Config()` - Dòng 235-252

```c
PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SMPS | RCC_PERIPHCLK_RFWAKEUP;
PeriphClkInitStruct.RFWakeUpClockSelection = RCC_RFWKPCLKSOURCE_HSE_DIV1024;
PeriphClkInitStruct.SmpsClockSelection = RCC_SMPSCLKSOURCE_HSI;
PeriphClkInitStruct.SmpsDivSelection = RCC_SMPSCLKDIV_RANGE0;
```

**Cấu hình**:
- **SMPS** (Switched Mode Power Supply): Dùng HSI
- **RF Wake-up Clock**: HSE / 1024 (cho power consumption thấp khi wake-up)

---

## 6. PERIPHERAL INITIALIZATION FUNCTIONS

### 6.1 `MX_IPCC_Init()` - Dòng 254-272

**Chức năng**: Khởi tạo Inter-Processor Communication Controller

```c
hipcc.Instance = IPCC;
if (HAL_IPCC_Init(&hipcc) != HAL_OK)
    Error_Handler();
```

**API**:
- `HAL_IPCC_Init()`: Khởi tạo IPCC controller
- `IPCC`: Instance là thanh ghi module IPCC

**Ý nghĩa**:
- IPCC là phần cứng cho phép CPU1 và CPU2 giao tiếp
- BLE Stack chạy trên CPU2 cần IPCC để gửi events đến CPU1

---

### 6.2 `MX_LPUART1_UART_Init()` - Dòng 274-312

**Chức năng**: Khởi tạo UART tốc độ thấp để debug

```c
hlpuart1.Instance = LPUART1;
hlpuart1.Init.BaudRate = 115200;
hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
hlpuart1.Init.StopBits = UART_STOPBITS_1;
hlpuart1.Init.Parity = UART_PARITY_NONE;
hlpuart1.Init.Mode = UART_MODE_TX_RX;
hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
```

**Cấu hình chi tiết**:
| Tham số | Giá trị | Ý nghĩa |
|--------|--------|---------|
| BaudRate | 115200 | 115.2 kbps - chuẩn debug |
| WordLength | 8B | 8-bit dữ liệu |
| StopBits | 1 | 1 bit stop |
| Parity | NONE | Không có parity bit |
| Mode | TX_RX | Hai chiều |
| HwFlowCtl | NONE | Không flow control |
| FifoMode | DISABLE | FIFO bị vô hiệu |

**API sử dụng**:
- `HAL_UART_Init()`: Khởi tạo UART với cấu hình
- `HAL_UARTEx_SetTxFifoThreshold()`: Cấu hình TX FIFO (1/8 full)
- `HAL_UARTEx_SetRxFifoThreshold()`: Cấu hình RX FIFO (1/8 full)
- `HAL_UARTEx_DisableFifoMode()`: Tắt FIFO (để đơn giản)

---

### 6.3 `MX_RNG_Init()` - Dòng 335-353

**Chức năng**: Khởi tạo Random Number Generator

```c
hrng.Instance = RNG;
hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
if (HAL_RNG_Init(&hrng) != HAL_OK)
    Error_Handler();
```

**Cấu hình**:
- **ClockErrorDetection**: BẬT - Phát hiện lỗi xung (bảo mật)

**API**:
- `HAL_RNG_Init()`: Khởi tạo RNG hardware

**Ứng dụng**:
- Sinh số ngẫu nhiên cho mã hóa BLE
- Chia sẻ entropy cho BLE Stack

---

### 6.4 `MX_RTC_Init()` - Dòng 355-385

**Chức năng**: Khởi tạo Real Time Clock

```c
hrtc.Instance = RTC;
hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
hrtc.Init.AsynchPrediv = CFG_RTC_ASYNCH_PRESCALER;
hrtc.Init.SynchPrediv = CFG_RTC_SYNCH_PRESCALER;
hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
```

**Cấu hình chi tiết**:
- **HourFormat**: 24-hour
- **AsynchPrediv/SynchPrediv**: Chia tần số (từ LSE 32.768 kHz)
- **Output**: Tắt đầu ra RTC

**API**:
- `HAL_RTC_Init()`: Khởi tạo RTC

**Ứng dụng**:
- Timekeeping
- Timestamps cho BLE events
- Wake-up alarms

---

### 6.5 `MX_RF_Init()` - Dòng 314-326

**Chức năng**: Khởi tạo RF (Radio Frequency)

```c
static void MX_RF_Init(void)
{
  /* USER CODE BEGIN RF_Init 0 */
  /* USER CODE END RF_Init 0 */
  /* USER CODE BEGIN RF_Init 1 */
  /* USER CODE END RF_Init 1 */
  /* USER CODE BEGIN RF_Init 2 */
  /* USER CODE END RF_Init 2 */
}
```

**Ghi chú**: Hàm này để TRỐNG vì RF được quản lý bởi BLE Stack trên CPU2

---

### 6.6 `MX_GPIO_Init()` - Dòng 387-437

**Chức năng**: Cấu hình tất cả GPIO pins

#### a. Bật clock cho GPIO ports - Dòng 397-400
```c
__HAL_RCC_GPIOC_CLK_ENABLE();
__HAL_RCC_GPIOB_CLK_ENABLE();
__HAL_RCC_GPIOA_CLK_ENABLE();
__HAL_RCC_GPIOD_CLK_ENABLE();
```

#### b. Cấu hình Output - Dòng 402-403 & 410-415
```c
HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_5, GPIO_PIN_RESET);
// SET = Low, sau này được toggle để bật LED

GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_5;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;      // Push-Pull output
GPIO_InitStruct.Pull = GPIO_NOPULL;              // Không pull-up/down
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;     // Tốc độ thấp = tiết kiệm điện
HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
```

**Ý nghĩa**:
- 3 LED được nối tại PB0, PB1, PB5
- Output Push-Pull = có thể drive mạnh cả HIGH và LOW
- Speed LOW = nhấp nháy LED không cần tốc độ cao

#### c. Cấu hình USB Pins - Dòng 417-423
```c
GPIO_InitStruct.Pin = USB_DM_Pin|USB_DP_Pin;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;              // Alternate Function
GPIO_InitStruct.Alternate = GPIO_AF10_USB;          // AF10 = USB
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

**Ý nghĩa**:
- USB Data+/Data- được map vào PA11/PA12
- AF (Alternate Function) = Chân này được điều khiển bởi module USB

#### d. Cấu hình Input Pins - Dòng 405-408 & 425-430
```c
GPIO_InitStruct.Pin = GPIO_PIN_4;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_NOPULL;
HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
```

**Ý nghĩa**:
- PC4, PD0, PD1 là input (có thể là cảm biến hoặc status pins)
- NOPULL = không pull, rely trên external circuit

**API sử dụng**:
- `__HAL_RCC_GPIOx_CLK_ENABLE()`: Bật clock cho port
- `HAL_GPIO_WritePin()`: Set giá trị pin
- `HAL_GPIO_Init()`: Cấu hình pin

---

## 7. EXTERNAL INTERRUPT CALLBACK

### 7.1 `HAL_GPIO_EXTI_Callback()` - Dòng 442-461

**Chức năng**: Xử lý interrupt khi nút bấm được nhấn

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch(GPIO_Pin) {
    case BUTTON_SW1_PIN:
      delay = 100;   // Nhấp nháy nhanh
      break;
    case BUTTON_SW2_PIN:
      delay = 500;   // Nhấp nháy bình thường
      break;
    case BUTTON_SW3_PIN:
      delay = 1000;  // Nhấp nháy chậm
      break;
    default:
      break;
  }
}
```

**Cách hoạt động**:
1. Khi nút được nhấn → GPIO interrupt được trigger
2. HAL driver tự động gọi `HAL_GPIO_EXTI_Callback()`
3. Hàm thay đổi biến global `delay`
4. Vòng lặp main sử dụng giá trị `delay` mới

**API sử dụng**:
- `HAL_GPIO_EXTI_Callback()`: Callback được gọi bởi IRQ handler

---

## 8. ERROR HANDLING

### 8.1 `Error_Handler()` - Dòng 463-473

```c
void Error_Handler(void)
{
  __disable_irq();  // Tắt tất cả interrupt
  while (1) {       // Hang (chờ reset)
  }
}
```

**Chức năng**:
- Được gọi khi có lỗi khởi tạo
- Tắt interrupt để tránh hung interrupt
- Infinite loop chờ manual reset hoặc watchdog reset

**Khi nào gọi**:
- `HAL_Init()` fail
- Clock config fail
- Peripheral init fail
- IPCC init fail

---

## 9. ASSERT FUNCTION

### 9.1 `assert_failed()` - Dòng 475-485

```c
#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  // Được gọi bởi assert_param() macros
}
#endif
```

**Chức náng**:
- Hỗ trợ kiểm tra tham số của HAL functions
- Chỉ có hiệu lực khi `USE_FULL_ASSERT` được define

---

## 10. TÓM TẮT CÁC API HAL CHÍNH

| API | Chức năng | Loại |
|-----|----------|------|
| `HAL_Init()` | Reset peripherals, init Flash & Systick | Khởi tạo |
| `HAL_RCC_OscConfig()` | Cấu hình oscillators | Clock |
| `HAL_RCC_ClockConfig()` | Cấu hình clock dividers | Clock |
| `HAL_PWR_EnableBkUpAccess()` | Mở khóa Backup Domain | Power |
| `HAL_IPCC_Init()` | Khởi tạo IPCC | IPC |
| `HAL_UART_Init()` | Khởi tạo UART | Serial |
| `HAL_UART_Transmit()` | Gửi dữ liệu UART | Serial |
| `HAL_RNG_Init()` | Khởi tạo RNG | Random |
| `HAL_RTC_Init()` | Khởi tạo RTC | Time |
| `HAL_GPIO_Init()` | Cấu hình GPIO | GPIO |
| `HAL_GPIO_WritePin()` | Ghi giá trị GPIO | GPIO |
| `HAL_Delay()` | Trì hoãn (blocking) | Time |

---

## 11. FLOW DIAGRAM CỦA CHƯƠNG TRÌNH

```
START
  ↓
HAL_Init() [Reset all, init Flash/Systick]
  ↓
MX_APPE_Config() [BLE config level 1]
  ↓
SystemClock_Config() [Set SYSCLK=64MHz]
  ↓
PeriphCommonClock_Config() [Config SMPS, RF clock]
  ↓
MX_IPCC_Init() [Init CPU1↔CPU2 communication]
  ↓
Initialize Peripherals [GPIO, UART, RNG, RTC, RF]
  ↓
Force C2BOOT Enable [Ensure CPU2 can boot]
  ↓
MX_APPE_Init() [BLE stack full init level 2]
  ↓
BSP Init [LED, Button with EXTI interrupts]
  ↓
Turn ON all LEDs
  ↓
WHILE(1):
  ├─ Toggle LED_BLUE + HAL_Delay(delay)
  ├─ Toggle LED_GREEN + HAL_Delay(delay)
  ├─ Toggle LED_RED + HAL_Delay(delay)
  ├─ MX_APPE_Process() [Handle BLE events from CPU2]
  └─ (EXTI callback changes 'delay' when button pressed)
```

---

## 12. GỌISYSTEM INITIALIZATION SEQUENCE CHI TIẾT

```
Clock System (Power-on Reset)
  ├─ HSI (internal 16MHz) + PLL OFF → SYSCLK từ HSI
  ├─ MSI + HSE + LSE bị tắt

SystemClock_Config():
  ├─ Enable HSE, HSI, MSI, LSE, LSI, PLL
  ├─ PLL: MSI(~4MHz) × 32 ÷ 2 = 64MHz
  └─ SYSCLK ← PLL (64MHz)

PeriphCommonClock_Config():
  ├─ SMPS clock ← HSI
  └─ RF wakeup ← HSE ÷ 1024

Peripheral Clocks:
  ├─ HCLK1 (CPU1) = 64MHz
  ├─ HCLK2 (CPU2/M0+) = 32MHz
  ├─ HCLK4 (AHB4) = 64MHz
  ├─ PCLK1 (APB1) = 64MHz
  └─ PCLK2 (APB2) = 64MHz
```

---

## 13. ĐIỂM QUAN TRỌNG (CRITICAL POINTS)

### ⚠️ C2BOOT Khẩn Cấp
- CPU2 không khởi động nếu C2BOOT chưa bật
- Workaround: Ghi trực tiếp vào `PWR->CR4`
- Cách lý tưởng: Dùng STM32CubeMX để cấu hình Option Bytes

### ⚠️ HSE Tuning
- `MX_APPE_Config()` PHẢI trước `SystemClock_Config()`
- HSE tuning cần thiết cho RF accuracy

### ⚠️ IPCC Initialization
- Phải init trước BLE (vì CPU2 giao tiếp qua IPCC)

### ⚠️ UART Debug
- Baud 115200 là chuẩn
- `_write()` hook cho `printf()`

---

## 14. THAY ĐỔI DELAY - BUTTON INTEROPERABILITY

**Trạng thái ban đầu**: `delay = 250ms`

| Nút bấm | Hành động | Kết quả |
|---------|-----------|---------|
| SW1 | Nhấn | LED nhấp nháy 100ms/lần (nhanh) |
| SW2 | Nhấn | LED nhấp nháy 500ms/lần (bình thường) |
| SW3 | Nhấn | LED nhấp nháy 1000ms/lần (chậm) |

**Cơ chế**:
- Button nhấn → GPIO interrupt → EXTI callback
- Callback thay đổi global `delay`
- Main loop dùng giá trị `delay` mới (ngay lần lặp tiếp theo)

---

## 15. BLE STACK ARCHITECTURE & INITIALIZATION

### 15.1 Kiến trúc Dual-Core STM32WB55

```
┌─────────────────────────────────────────────┐
│         STM32WB55 (Dual Core)              │
├──────────────────┬──────────────────────────┤
│   CPU1 (M4)      │      CPU2 (M0+)         │
├──────────────────┼──────────────────────────┤
│ • main()         │ • BLE Stack              │
│ • App Logic      │ • RF Driver              │
│ • Peripherals    │ • Scheduler              │
│ • GPIO/UART      │ • Memory Manager         │
└──────────────────┴──────────────────────────┘
         ↕ IPCC (Inter-Processor Communication Controller)
         ↕ SHCI (System Host Command Interface)
```

### 15.2 MX_APPE_Config() - Cấu hình sơ cấp BLE

**Vị trí**: `app_entry.c` - Dòng ~98

```c
void MX_APPE_Config(void)
{
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);
  Reset_Device();
  Config_HSE();
  return;
}
```

**Chức năng**:
- **Clear FLASH OPTVERR flag**: Lỗi khi power-on, cần xóa trước dùng HAL
- **Reset_Device()**: Reset IPCC, Backup Domain (nếu CFG_HW_RESET_BY_FW=1)
- **Config_HSE()**: Đọc HSE tuning value từ OTP memory và áp dụng

**API**:
- `__HAL_FLASH_CLEAR_FLAG()`: Xóa flag lỗi
- `OTP_Read()`: Đọc One-Time Programmable memory
- `LL_RCC_HSE_SetCapacitorTuning()`: Cấu hình HSE capacitor tuning

**Tại sao HSE tuning quan trọng**:
- BLE RF cần HSE (32 MHz) xác định cao
- OTP chứa giá trị tuning được calibrated tại factory
- Không đúng tuning → RF không chính xác → kết nối BLE thất bại

### 15.3 MX_APPE_Init() - Khởi tạo chính BLE Stack

**Vị trí**: `app_entry.c` - Dòng ~111

```c
void MX_APPE_Init(void)
{
  System_Init();           // Init SMPS, EXTI, RTC
  SystemPower_Config();    // Configure Low Power Mode
  HW_TS_Init(...);         // Initialize TimerServer
  appe_Tl_Init();          // Initialize Transport Layers (SHCI_TL)
  return;
}
```

**Các giai đoạn**:

#### a. System_Init()
```c
static void System_Init(void)
{
  Init_Smps();    // Switched Mode Power Supply
  Init_Exti();    // Enable IPCC/HSEM EXTI interrupts
  Init_Rtc();     // Configure RTC wake-up
}
```

**Init_Smps()**:
```c
void Init_Smps(void)
{
  LL_PWR_SMPS_SetStartupCurrent(LL_PWR_SMPS_STARTUP_CURRENT_80MA);
  LL_PWR_SMPS_SetOutputVoltageLevel(LL_PWR_SMPS_OUTPUT_VOLTAGE_1V40);
  LL_PWR_SMPS_Enable();
}
```
- SMPS = Bộ cấp nguồn switching để giảm nhiệt, tăng tuổi thọ pin
- Startup current: 80mA
- Output voltage: 1.4V (giới hạn RF power đến 3.7dBm)
- Để tăng RF power, tăng voltage lên 1.8V-2.0V

**Init_Exti()**:
```c
void Init_Exti(void)
{
  LL_EXTI_EnableIT_32_63(LL_EXTI_LINE_36 | LL_EXTI_LINE_38);
}
```
- Line 36 = IPCC interrupt (CPU1 nhận thông điệp từ CPU2)
- Line 38 = HSEM (Hardware Semaphore) - task scheduling

**Init_Rtc()**:
```c
static void Init_Rtc(void)
{
  LL_RTC_DisableWriteProtection(RTC);
  LL_RTC_WAKEUP_SetClock(RTC, CFG_RTC_WUCKSEL_DIVIDER);
  LL_RTC_EnableWriteProtection(RTC);
}
```
- Cấu hình RTC wake-up clock divider (thường /16 từ LSE)

#### b. SystemPower_Config()
```c
static void SystemPower_Config(void)
{
  LL_RCC_SetClkAfterWakeFromStop(LL_RCC_STOP_WAKEUPCLOCK_HSI);
  UTIL_LPM_Init();  // Initialize Low Power Manager
  LL_C2_PWR_SetPowerMode(LL_PWR_MODE_SHUTDOWN);
}
```
- HSI được dùng khi CPU1 wake-up từ STOP mode (nhanh hơn HSE)
- UTIL_LPM_Init(): Khởi tạo low power manager
- CPU2 mode: SHUTDOWN (tối thiểu điện năng khi CPU1 sleep)

#### c. appe_Tl_Init() - Khởi tạo Transport Layer
```c
static void appe_Tl_Init(void)
{
  // Initialize SHCI_TL (System HCI Transport Layer)
  // Setup mailbox, buffer pools, callbacks
  // Mở IPCC channels cho giao tiếp CPU1 ↔ CPU2
}
```
- TL = Transport Layer (lớp giao tiếp vật lý)
- SHCI = System Host Command Interface (lệnh hệ thống)
- Mailbox: Vùng nhớ chia sẻ giữa 2 CPU (MB_MEM1, MB_MEM2)

### 15.4 MX_APPE_Process() - Xử lý BLE Events

**Vị trí**: `app_entry.c`, gọi từ `main()` vòng lặp chính

```c
void MX_APPE_Process(void)
{
  // Được gọi liên tục từ main while(1) loop
  // Xử lý các sự kiện BLE từ queue
  // Gọi callbacks cho HCI events, BLE events
}
```

**Chức năng**:
- Poll BLE event queue (do CPU2 gửi qua IPCC)
- Gọi BLE_UserEvtRx() callback để xử lý
- Cho phép ứng dụng react đến BLE connection, advertisement events

**Flow**:
```
Vòng lặp main:
  MX_APPE_Process()
    ↓
  Kiểm tra IPCC mailbox
    ↓
  Có event từ CPU2?
    ↓
  Ghi event vào event queue
    ↓
  Gọi BLE_UserEvtRx(event)
      ↓
    Xử lý:
    - Connection established
    - Notification received
    - Gatt discovered
    - Advertisement report
    - Etc...
```

### 15.5 APP_BLE_Init() - Khởi tạo Application BLE

**Vị trí**: `app_ble.c` - Dòng ~250

```c
void APP_BLE_Init(void)
{
  SHCI_C2_Ble_Init_Cmd_Packet_t ble_init_cmd_packet = {
    {0, 0, 0},                    // Header
    {
      0,                          // pBleBufferAddress
      0,                          // BleBufferSize
      CFG_BLE_NUM_GATT_ATTRIBUTES,
      CFG_BLE_NUM_GATT_SERVICES,
      CFG_BLE_ATT_VALUE_ARRAY_SIZE,
      CFG_BLE_NUM_LINK,           // Số kết nối: thường 1-2
      CFG_BLE_DATA_LENGTH_EXTENSION,
      CFG_BLE_PREPARE_WRITE_LIST_SIZE,
      CFG_BLE_MBLOCK_COUNT,
      CFG_BLE_MAX_ATT_MTU,        // Maximum ATT MTU
      CFG_BLE_PERIPHERAL_SCA,     // Peripheral Sleep Clock Accuracy
      CFG_BLE_CENTRAL_SCA,        // Central Sleep Clock Accuracy
      CFG_BLE_LS_SOURCE,          // Low Speed clock source
      CFG_BLE_MAX_CONN_EVENT_LENGTH,
      CFG_BLE_HSE_STARTUP_TIME,
      CFG_BLE_VITERBI_MODE,       // RF Demodulation mode
      CFG_BLE_OPTIONS,
      CFG_BLE_MAX_COC_INITIATOR_NBR,  // L2CAP CoC
      CFG_BLE_MIN_TX_POWER,
      CFG_BLE_MAX_TX_POWER,
      CFG_BLE_RX_MODEL_CONFIG,
      CFG_BLE_MAX_ADV_SET_NBR,    // Số advertisement sets
      CFG_BLE_MAX_ADV_DATA_LEN,   // Độ dài advertisement data
      CFG_BLE_TX_PATH_COMPENS,    // TX path compensation
      CFG_BLE_RX_PATH_COMPENS,    // RX path compensation
      CFG_BLE_CORE_VERSION,
      CFG_BLE_OPTIONS_EXT
    }
  };
  
  Ble_Tl_Init();                  // Initialize BLE Transport Layer
  UTIL_LPM_SetOffMode(...);       // Disable standby during app BLE
  
  // Gửi BLE_Init command đến CPU2 qua SHCI
  SHCI_C2_BLE_Init(&ble_init_cmd_packet);
}
```

**Cấu hình quan trọng**:

| Tham số | Ý nghĩa | Mặc định |
|---------|---------|---------|
| NUM_GATT_ATTRIBUTES | Số GATT attributes tối đa | 10-20 |
| NUM_GATT_SERVICES | Số GATT services | 3-5 |
| NUM_LINK | Số BLE connections | 1-2 |
| MAX_ATT_MTU | Kích thước ATT packet | 247 |
| MAX_TX_POWER | TX power dBm | 0 (1mW) |
| MAX_ADV_SET_NBR | Số extended advertisements | 1-4 |
| VITERBI_MODE | Chế độ demodulation | 1 (performance) |

### 15.6 BLE Connection States

**Enum từ app_ble.h**:
```c
typedef enum {
  APP_BLE_IDLE,                           // Không hoạt động
  APP_BLE_FAST_ADV,                       // Quảng cáo nhanh (20ms interval)
  APP_BLE_LP_ADV,                         // Quảng cáo chậm (1-2s interval)
  APP_BLE_SCAN,                           // Quét (central role)
  APP_BLE_LP_CONNECTING,                  // Đang kết nối
  APP_BLE_CONNECTED_SERVER,               // Kết nối (Peripheral role)
  APP_BLE_CONNECTED_CLIENT,               // Kết nối (Central role)
  APP_BLE_DISCOVER_SERVICES,              // Khám phá GATT services
  APP_BLE_DISCOVER_CHARACS,               // Khám phá GATT characteristics
  APP_BLE_DISCOVER_WRITE_DESC,            // Khám phá descriptors
  APP_BLE_DISCOVER_NOTIFICATION_CHAR_DESC,
  APP_BLE_ENABLE_NOTIFICATION_DESC,       // Bật notifications
  APP_BLE_DISABLE_NOTIFICATION_DESC       // Tắt notifications
} APP_BLE_ConnStatus_t;
```

### 15.7 Security Parameters

**Struct từ app_ble.c**:
```c
typedef struct {
  uint8_t ioCapability;          // IO capability (keyboard, display)
  uint8_t mitm_mode;             // MITM protection required
  uint8_t bonding_mode;          // Bonding (lưu trữ key)
  uint8_t Use_Fixed_Pin;         // 0=fixed pin, 1=passkey input
  uint8_t encryptionKeySizeMin;  // Min 7 bytes
  uint8_t encryptionKeySizeMax;  // Max 16 bytes
  uint32_t Fixed_Pin;            // PIN code (nếu use fixed)
  uint8_t initiateSecurity;      // Host initiate security?
} tSecurityParams;
```

### 15.8 BLE Global Context

```c
typedef struct {
  uint16_t gapServiceHandle;     // GAP service handle
  uint16_t devNameCharHandle;    // Device name char handle
  uint16_t appearanceCharHandle; // Appearance char handle
  uint16_t connectionHandle;     // Current connection handle
  uint8_t advtServUUIDlen;       // Length of UUID list
  uint8_t advtServUUID[100];     // UUID array
} BleGlobalContext_t;
```

### 15.9 BLE Security - IR & ER Keys

```c
// Identity Root Key (dùng để derive IRK - Identity Resolving Key)
static const uint8_t a_BLE_CfgIrValue[16] = CFG_BLE_IR;

// Encryption Root Key (dùng để derive LTK - Long Term Key)
static const uint8_t a_BLE_CfgErValue[16] = CFG_BLE_ER;
```

**Ý nghĩa**:
- IR: Dùng cho Identity privacy (che giấu MAC address)
- ER: Dùng cho encryption key negotiation
- Phải định nghĩa trong configuration

### 15.10 IPCC Channels cho BLE

```
IPCC có 6 channels (1-6):

Channel 1: BLE HCI Command
  CPU1 → CPU2: HCI commands (GAP, GATT, Bonding)
  
Channel 2: System HCI Command
  CPU1 → CPU2: System commands (RNG, OTA)
  
Channel 3: BLE HCI Event
  CPU2 → CPU1: BLE events (Connection, Notifications)
  
Channel 4: System HCI Event
  CPU2 → CPU1: System events (Ready, Error)
  
Channel 5: BLE ACL Data
  CPU1 ↔ CPU2: L2CAP data (variable length)
  
Channel 6: Event Queue (optional)
```

### 15.11 Mailbox Memory Layout

```
MB_MEM1 (16 KB, CPU1 write/CPU2 read):
  - BLE Command buffer
  - System Command buffer
  - TX ACL data

MB_MEM2 (32 KB, CPU2 write/CPU1 read):
  - BLE Event buffers (queue)
  - System Event buffers
  - RX ACL data
  - BLE Stack state

IPCC Channel Status:
  - Flag: Dữ liệu sẵn sàng ở mailbox
  - Interrupt: Khi flag được set
```

---

## 16. NGUỒN & TÀI LIỆU

- **STM32WB55 Reference Manual**: Thanh ghi, memory map, IPCC
- **STM32WB55 HAL Driver**: API documentation
- **STM32_WPAN Middleware**: BLE Stack, SHCI, TL
- **NUCLEO-WB55 Board Support**: LED/Button definitions
- **BLE Core Specification 5.x**: BLE protocol details
- **app_entry.c, app_ble.c**: Implementation examples
