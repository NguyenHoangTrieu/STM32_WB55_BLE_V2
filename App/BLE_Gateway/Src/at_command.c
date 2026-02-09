/**
  ******************************************************************************
  * @file    at_command.c
  * @brief   AT Command Parser implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "at_command.h"
#include "ble_device_manager.h"
#include "ble_connection.h"
#include "ble_gatt_client.h"
#include "debug_trace.h"
#include "module_system.h"
#include "module_config.h"
#include "module_power.h"
#include "module_mode.h"
#include "main.h"
#include "app_conf.h"
#include "stm32_seq.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

extern UART_HandleTypeDef hlpuart1;

/*============================================================================
 * Constants
 *============================================================================*/
#define AT_RX_TIMEOUT_MS    500U
#define AT_MAX_GARBAGE      20U
#define ASCII_SPACE         0x20
#define ASCII_TILDE         0x7E
#define ASCII_CR            0x0D
#define ASCII_LF            0x0A

/*============================================================================
 * AT Command Line Buffer (accessed by ISR)
 *============================================================================*/
static volatile char at_line_buf[AT_CMD_MAX_LEN];
static volatile uint16_t at_line_idx = 0;
static volatile uint8_t at_cmd_ready = 0;
static volatile uint16_t at_garbage_count = 0;

/* Double buffer for safe processing */
static char at_cmd_buf[AT_CMD_MAX_LEN];

/* Simple tick counter for timeout (incremented in ISR) */
static volatile uint32_t at_rx_tick = 0;

/*============================================================================
 * Static Helper Functions
 *============================================================================*/

/**
 * @brief Parse unsigned decimal string to uint16_t
 * @return Parsed value, or 0 if invalid
 */
static uint16_t ParseUInt16(const char *str)
{
    uint32_t val = 0;
    
    if (str == NULL || *str == '\0') {
        return 0;
    }
    
    while (*str >= '0' && *str <= '9') {
        val = val * 10U + (uint32_t)(*str - '0');
        if (val > 0xFFFFU) {
            return 0;  /* Overflow */
        }
        str++;
    }
    
    return (uint16_t)val;
}

/**
 * @brief Parse unsigned decimal string to uint8_t
 * @return Parsed value, or 0xFF if invalid
 */
static uint8_t ParseUInt8(const char *str)
{
    uint16_t val = 0;
    
    if (str == NULL || *str == '\0') {
        return 0xFF;
    }
    
    while (*str >= '0' && *str <= '9') {
        val = val * 10U + (uint16_t)(*str - '0');
        if (val > 0xFFU) {
            return 0xFF;  /* Overflow */
        }
        str++;
    }
    
    return (uint8_t)val;
}

/**
 * @brief Skip to next comma in string
 * @return Pointer to character after comma, or NULL
 */
static const char* SkipToComma(const char *str)
{
    if (str == NULL) {
        return NULL;
    }
    
    while (*str != '\0' && *str != ',') {
        str++;
    }
    
    if (*str == ',') {
        return str + 1;  /* Skip the comma */
    }
    
    return NULL;
}

/**
 * @brief Parse hex nibble character to value
 * @return 0-15 if valid, 0xFF if invalid
 */
static uint8_t ParseHexNibble(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0xFF;
}

/**
 * @brief Parse hex string "AABBCC..." to bytes
 * @param hex_str Hex string (must have even length)
 * @param out_bytes Output buffer
 * @param max_len Maximum output buffer size
 * @return Number of bytes parsed, or -1 if error
 */
static int ParseHexString(const char *hex_str, uint8_t *out_bytes, uint16_t max_len)
{
    uint16_t i = 0;
    uint8_t hi, lo;
    
    if (hex_str == NULL || out_bytes == NULL || max_len == 0) {
        return -1;
    }

    size_t hex_len = strlen(hex_str);
    if (hex_len % 2 != 0) {
        return -1; // Odd number of chars
    }
    if (hex_len / 2 > max_len) {
        return -1; // Exceeds buffer size
    }
    
    while (hex_str[0] != '\0' && hex_str[1] != '\0' && i < max_len) {
        hi = ParseHexNibble(hex_str[0]);
        lo = ParseHexNibble(hex_str[1]);
        
        if (hi == 0xFF || lo == 0xFF) {
            return -1;  /* Invalid hex character */
        }
        
        out_bytes[i] = (hi << 4) | lo;
        i++;
        hex_str += 2;
    }
    
    /* Check if there's a remaining unpaired nibble */
    if (hex_str[0] != '\0' && hex_str[1] == '\0') {
        return -1;  /* Odd number of hex characters */
    }
    
    return (int)i;
}

/**
 * @brief Parse MAC string "AA:BB:CC:DD:EE:FF" to bytes
 * @note  Simple parser without sscanf for embedded efficiency
 */
static int ParseMACString(const char *mac_str, uint8_t *mac_bytes)
{
    if (mac_str == NULL || mac_bytes == NULL) {
        return -1;
    }
    
    /* Expected format: "XX:XX:XX:XX:XX:XX" = 17 chars */
    uint8_t i;
    for (i = 0; i < 6U; i++) {
        uint8_t hi, lo;
        char c;
        
        /* High nibble */
        c = mac_str[i * 3U];
        if (c >= '0' && c <= '9')      hi = (uint8_t)(c - '0');
        else if (c >= 'A' && c <= 'F') hi = (uint8_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') hi = (uint8_t)(c - 'a' + 10);
        else return -1;
        
        /* Low nibble */
        c = mac_str[i * 3U + 1U];
        if (c >= '0' && c <= '9')      lo = (uint8_t)(c - '0');
        else if (c >= 'A' && c <= 'F') lo = (uint8_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') lo = (uint8_t)(c - 'a' + 10);
        else return -1;
        
        mac_bytes[i] = (hi << 4) | lo;
        
        /* Check separator (except for last byte) */
        if (i < 5U && mac_str[i * 3U + 2U] != ':') {
            return -1;
        }
    }
    
    return 0;
}

/*============================================================================
 * AT Command Initialization
 *============================================================================*/
void AT_Command_Init(void)
{
    at_line_idx = 0;
    at_cmd_ready = 0;
    at_garbage_count = 0;
    at_rx_tick = 0;
    memset((void*)at_line_buf, 0, sizeof(at_line_buf));
    memset(at_cmd_buf, 0, sizeof(at_cmd_buf));
    DEBUG_INFO("AT Command initialized");
}

/*============================================================================
 * ISR Byte Receive Handler
 * Called from LPUART1_IRQHandler - must be fast, no blocking!
 *============================================================================*/
void AT_Command_ReceiveByte(uint8_t byte)
{
    /* Increment simple tick counter */
    at_rx_tick++;
    
    /* If previous command not processed yet, drop new bytes */
    if (at_cmd_ready) {
        return;
    }
    
    /* Timeout check: reset if tick wrapped or too many ticks passed */
    if (at_line_idx > 0 && (at_rx_tick > AT_RX_TIMEOUT_MS)) {
        at_line_idx = 0;
        at_garbage_count = 0;
        at_rx_tick = 0;
    }
    
    /* Line terminator - command complete */
    if (byte == ASCII_CR || byte == ASCII_LF) {
        if (at_line_idx >= 2U) {
            at_line_buf[at_line_idx] = '\0';
            at_cmd_ready = 1;
            at_garbage_count = 0;
            at_rx_tick = 0;
            UTIL_SEQ_SetTask(1U << CFG_TASK_AT_CMD_PROC_ID, CFG_SCH_PRIO_0);
        } else {
            at_line_idx = 0;
        }
        return;
    }
    
    /* Filter: only printable ASCII */
    if (byte >= ASCII_SPACE && byte <= ASCII_TILDE) {
        if (at_line_idx < (AT_CMD_MAX_LEN - 1U)) {
            at_line_buf[at_line_idx] = (char)byte;
            at_line_idx++;
            at_rx_tick = 0;
            at_garbage_count = 0;
        }
    } else {
        at_garbage_count++;
        if (at_garbage_count > AT_MAX_GARBAGE) {
            at_line_idx = 0;
            at_garbage_count = 0;
        }
    }
}

/*============================================================================
 * Process Ready Command (called from sequencer task)
 *============================================================================*/
void AT_Command_ProcessReady(void)
{
    if (!at_cmd_ready) {
        return;
    }
    
    /* Critical section: copy buffer then reset ISR state */
    __disable_irq();
    memcpy(at_cmd_buf, (const void*)at_line_buf, at_line_idx + 1);
    at_line_idx = 0;
    at_cmd_ready = 0;
    __enable_irq();
    
    /* Process command outside critical section */
    AT_Command_Process(at_cmd_buf);
}

/*============================================================================
 * AT Response Send (to LPUART1)
 *============================================================================*/
void AT_Response_Send(const char *fmt, ...)
{
    static char response_buf[AT_CMD_MAX_LEN];
    va_list args;
    uint16_t len;
    
    va_start(args, fmt);
    len = (uint16_t)vsnprintf(response_buf, AT_CMD_MAX_LEN, fmt, args);
    va_end(args);
    
    if (len > AT_CMD_MAX_LEN) {
        len = AT_CMD_MAX_LEN;
    }
    
    /* Send via UART - blocking */
    HAL_UART_Transmit(&hlpuart1, (uint8_t *)response_buf, len, 100);
}

/*============================================================================
 * AT Command Parser
 *============================================================================*/
void AT_Command_Process(const char *cmd_line)
{
    char cmd[AT_CMD_MAX_LEN];
    uint16_t len;
    
    if (cmd_line == NULL || cmd_line[0] == '\0') {
        return;
    }
    
    /* Copy to local buffer */
    strncpy(cmd, cmd_line, AT_CMD_MAX_LEN - 1);
    cmd[AT_CMD_MAX_LEN - 1] = '\0';
    
    /* Remove trailing whitespace/newlines */
    len = (uint16_t)strlen(cmd);
    while (len > 0 && (cmd[len-1] == '\r' || cmd[len-1] == '\n' || cmd[len-1] == ' ')) {
        cmd[--len] = '\0';
    }
    
    /* Empty command after trim */
    if (len < 2) {
        return;  /* Too short, ignore silently */
    }
    
    /* CRITICAL: Command MUST start with "AT" (case insensitive) */
    if (!((cmd[0] == 'A' || cmd[0] == 'a') && (cmd[1] == 'T' || cmd[1] == 't'))) {
        /* Invalid command - not starting with AT, likely garbage */
        DEBUG_WARN("Invalid cmd (not AT): %s", cmd);
        return;  /* Don't send ERROR, just ignore garbage */
    }
    
    /* Debug log */
    DEBUG_PRINT("AT RX: %s", cmd);
    
    /* Parse commands */
    if (strcmp(cmd, "AT") == 0) {
        AT_Response_Send("OK\r\n");
    }
    else if (strncmp(cmd, "AT+SCAN", 7) == 0) {
        uint16_t duration = 5000U;  /* Default 5s */
        if (cmd[7] == '=' && cmd[8] != '\0') {
            uint16_t parsed = ParseUInt16(&cmd[8]);
            if (parsed > 0) {
                duration = parsed;
            }
        }
        AT_SCAN_Handler(duration);
    }
    else if (strcmp(cmd, "AT+STOP") == 0) {
        /* Stop scanning */
        if (BLE_Connection_StopScan() == 0) {
            AT_Response_Send("OK\r\n");
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strcmp(cmd, "AT+CLEAR") == 0) {
        /* Clear device list */
        BLE_DeviceManager_Clear();
        AT_Response_Send("OK\r\n");
    }
    else if (strncmp(cmd, "AT+LIST", 7) == 0) {
        AT_LIST_Handler();
    }
    else if (strncmp(cmd, "AT+CONNECT=", 11) == 0) {
        AT_CONNECT_Handler(cmd + 11);
    }
    else if (strncmp(cmd, "AT+DISCONNECT=", 14) == 0) {
        uint8_t idx = ParseUInt8(&cmd[14]);
        if (idx != 0xFFU) {
            AT_DISCONNECT_Handler(idx);
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+READ=", 8) == 0) {
        const char *p = &cmd[8];
        uint8_t idx = ParseUInt8(p);
        p = SkipToComma(p);
        if (p != NULL && idx != 0xFFU) {
            uint16_t handle = ParseUInt16(p);
            if (handle > 0) {
                AT_READ_Handler(idx, handle);
            } else {
                AT_Response_Send("ERROR\r\n");
            }
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+WRITE=", 9) == 0) {
        const char *p = &cmd[9];
        uint8_t idx = ParseUInt8(p);
        p = SkipToComma(p);
        if (p != NULL && idx != 0xFFU) {
            uint16_t handle = ParseUInt16(p);
            p = SkipToComma(p);
            if (p != NULL && handle > 0 && *p != '\0') {
                AT_WRITE_Handler(idx, handle, p);
            } else {
                AT_Response_Send("ERROR\r\n");
            }
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+NOTIFY=", 10) == 0) {
        const char *p = &cmd[10];
        uint8_t idx = ParseUInt8(p);
        p = SkipToComma(p);
        if (p != NULL && idx != 0xFFU) {
            uint16_t handle = ParseUInt16(p);
            p = SkipToComma(p);
            if (p != NULL && handle > 0) {
                uint8_t enable = ParseUInt8(p);
                if (enable != 0xFFU) {
                    AT_NOTIFY_Handler(idx, handle, enable);
                } else {
                    AT_Response_Send("ERROR\r\n");
                }
            } else {
                AT_Response_Send("ERROR\r\n");
            }
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+DISC=", 8) == 0) {
        uint8_t idx = ParseUInt8(&cmd[8]);
        if (idx != 0xFFU) {
            AT_DISC_Handler(idx);
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+INFO=", 8) == 0) {
        uint8_t idx = ParseUInt8(&cmd[8]);
        if (idx != 0xFFU) {
            AT_INFO_Handler(idx);
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    /* ============ System/Lifecycle Commands ============ */
    else if (strcmp(cmd, "AT+RESET") == 0) {
        AT_RESET_Handler();
    }
    else if (strcmp(cmd, "AT+HWRESET") == 0) {
        AT_HWRESET_Handler();
    }
    else if (strcmp(cmd, "AT+FACTORY") == 0) {
        AT_FACTORY_Handler();
    }
    /* ============ Info/Config Commands ============ */
    else if (strcmp(cmd, "AT+GETINFO") == 0) {
        AT_GETINFO_Handler();
    }
    else if (strncmp(cmd, "AT+NAME=", 8) == 0) {
        AT_NAME_Handler(&cmd[8]);
    }
    else if (strncmp(cmd, "AT+COMM=", 8) == 0) {
        /* Parse: AT+COMM=<baud>,<parity>,<stop> */
        const char *p = &cmd[8];
        uint32_t baud = (uint32_t)ParseUInt16(p);
        p = SkipToComma(p);
        if (p != NULL) {
            uint8_t parity = ParseUInt8(p);
            p = SkipToComma(p);
            if (p != NULL) {
                uint8_t stop = ParseUInt8(p);
                AT_COMM_Handler(baud, parity, stop);
            } else {
                AT_Response_Send("ERROR\r\n");
            }
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+RF=", 6) == 0) {
        /* Parse: AT+RF=<tx_power>,<scan_int>,<scan_win> */
        const char *p = &cmd[6];
        /* Parse as signed int for tx_power */
        int8_t tx_power = (int8_t)ParseUInt8(p);
        p = SkipToComma(p);
        if (p != NULL) {
            uint16_t scan_int = ParseUInt16(p);
            p = SkipToComma(p);
            if (p != NULL) {
                uint16_t scan_win = ParseUInt16(p);
                AT_RF_Handler(tx_power, scan_int, scan_win);
            } else {
                AT_Response_Send("ERROR\r\n");
            }
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strcmp(cmd, "AT+SAVE") == 0) {
        AT_SAVE_Handler();
    }
    /* ============ Mode Commands ============ */
    else if (strcmp(cmd, "AT+CMDMODE") == 0) {
        AT_CMDMODE_Handler();
    }
    else if (strncmp(cmd, "AT+DATAMODE=", 12) == 0) {
        /* Parse: AT+DATAMODE=<dev_idx>,<char_handle> */
        const char *p = &cmd[12];
        uint8_t idx = ParseUInt8(p);
        p = SkipToComma(p);
        if (p != NULL && idx != 0xFFU) {
            uint16_t handle = ParseUInt16(p);
            if (handle > 0) {
                AT_DATAMODE_Handler(idx, handle);
            } else {
                AT_Response_Send("ERROR\r\n");
            }
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    /* ============ Status Commands ============ */
    else if (strncmp(cmd, "AT+STATUS", 9) == 0) {
        uint8_t idx = 0xFF;  /* Default: all devices */
        if (cmd[9] == '=' && cmd[10] != '\0') {
            idx = ParseUInt8(&cmd[10]);
        }
        AT_STATUS_Handler(idx);
    }
    /* ============ Power Management Commands ============ */
    else if (strncmp(cmd, "AT+SLEEP", 8) == 0) {
        /* Parse: AT+SLEEP=<mode>,<wake_mask>,<timeout_ms> */
        uint8_t mode = 1;  /* Default: sleep mode */
        uint8_t wake_mask = 0x01;  /* Default: UART wake */
        uint32_t timeout_ms = 0;  /* Default: no timeout */
        
        if (cmd[8] == '=' && cmd[9] != '\0') {
            const char *p = &cmd[9];
            mode = ParseUInt8(p);
            p = SkipToComma(p);
            if (p != NULL) {
                wake_mask = ParseUInt8(p);
                p = SkipToComma(p);
                if (p != NULL) {
                    timeout_ms = (uint32_t)ParseUInt16(p);
                }
            }
        }
        AT_SLEEP_Handler(mode, wake_mask, timeout_ms);
    }
    else if (strcmp(cmd, "AT+WAKE") == 0) {
        AT_WAKE_Handler();
    }
    /* ============ Diagnostics Commands ============ */
    else if (strncmp(cmd, "AT+DIAG=", 8) == 0) {
        uint8_t idx = ParseUInt8(&cmd[8]);
        if (idx != 0xFFU) {
            AT_DIAG_Handler(idx);
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else {
        /* Unknown AT command - log but don't spam ERROR */
        DEBUG_WARN("Unknown AT cmd: %s", cmd);
    }
}

// ==================== AT Handlers ====================

int AT_SCAN_Handler(uint16_t duration_ms)
{
    int ret;
    
    DEBUG_INFO("AT+SCAN: duration=%dms", duration_ms);
    
    ret = BLE_Connection_StartScan(duration_ms);
    if (ret != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_CONNECT_Handler(const char *mac_str)
{
    uint8_t mac[6];
    int dev_idx;
    int ret;
    
    if (ParseMACString(mac_str, mac) != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    /* Device must be discovered first via scan */
    dev_idx = BLE_DeviceManager_FindDevice(mac);
    if (dev_idx < 0) {
        AT_Response_Send("+ERROR:NOT_FOUND\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+CONNECT: device %d", dev_idx);
    
    ret = BLE_Connection_CreateConnection(mac);
    if (ret != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    /* OK sent immediately, +CONNECTED will follow after HCI event */
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_DISCONNECT_Handler(uint8_t dev_idx)
{
    BLE_Device_t *dev;
    int ret;
    
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL || !dev->is_connected) {
        AT_Response_Send("+ERROR:NOT_CONNECTED\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+DISCONNECT: device %d, hdl=0x%04X", dev_idx, dev->conn_handle);
    
    ret = BLE_Connection_TerminateConnection(dev->conn_handle);
    if (ret != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    /* OK sent immediately, +DISCONNECTED will follow after HCI event */
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_LIST_Handler(void)
{
    uint8_t i, count;
    BLE_Device_t *dev;
    
    DEBUG_INFO("AT+LIST");
    
    count = BLE_DeviceManager_GetCount();
    AT_Response_Send("+LIST:%d\r\n", (int)count);
    
    for (i = 0; i < count; i++) {
        dev = BLE_DeviceManager_GetDevice((int)i);
        if (dev != NULL) {
            AT_Response_Send("+DEV:%d,%02X:%02X:%02X:%02X:%02X:%02X,%d,0x%04X,%s\r\n",
                (int)i,
                dev->mac_addr[5], dev->mac_addr[4], dev->mac_addr[3],
                dev->mac_addr[2], dev->mac_addr[1], dev->mac_addr[0],
                (int)dev->rssi,
                dev->conn_handle,
                (dev->name[0] != '\0') ? dev->name : "Unknown");
        }
    }
    
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_READ_Handler(uint8_t dev_idx, uint16_t char_handle)
{
    BLE_Device_t *dev;
    int ret;
    
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL || !dev->is_connected) {
        AT_Response_Send("+ERROR:NOT_CONNECTED\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+READ: dev=%d, handle=0x%04X", dev_idx, char_handle);
    
    /* Initiate read - response will come async via GATT event */
    ret = BLE_GATT_ReadCharacteristic(dev->conn_handle, char_handle);
    if (ret != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    /* OK sent immediately, +READ response will follow after GATT event */
    AT_Response_Send("OK\r\n");
    return 0;
}

#define AT_WRITE_MAX_DATA_LEN  64U

int AT_WRITE_Handler(uint8_t dev_idx, uint16_t char_handle, const char *data)
{
    BLE_Device_t *dev;
    uint8_t write_buf[AT_WRITE_MAX_DATA_LEN];
    int data_len;
    int ret;
    
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL || !dev->is_connected) {
        AT_Response_Send("+ERROR:NOT_CONNECTED\r\n");
        return -1;
    }
    
    if (data == NULL || data[0] == '\0') {
        AT_Response_Send("+ERROR:NO_DATA\r\n");
        return -1;
    }
    
    /* Parse hex string to bytes */
    data_len = ParseHexString(data, write_buf, AT_WRITE_MAX_DATA_LEN);
    if (data_len <= 0) {
        AT_Response_Send("+ERROR:INVALID_HEX\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+WRITE: dev=%d, handle=0x%04X, len=%d", dev_idx, char_handle, data_len);
    
    ret = BLE_GATT_WriteCharacteristic(dev->conn_handle, char_handle, write_buf, (uint16_t)data_len);
    if (ret != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    /* OK sent immediately, +WRITE response will follow after GATT proc complete */
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_NOTIFY_Handler(uint8_t dev_idx, uint16_t desc_handle, uint8_t enable)
{
    BLE_Device_t *dev;
    int ret;
    
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL || !dev->is_connected) {
        AT_Response_Send("+ERROR:NOT_CONNECTED\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+NOTIFY: dev=%d, handle=0x%04X, enable=%d", dev_idx, desc_handle, enable);
    
    if (enable) {
        ret = BLE_GATT_EnableNotification(dev->conn_handle, desc_handle);
    } else {
        ret = BLE_GATT_DisableNotification(dev->conn_handle, desc_handle);
    }
    
    if (ret != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_DISC_Handler(uint8_t dev_idx)
{
    BLE_Device_t *dev;
    int ret;
    
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL || !dev->is_connected) {
        AT_Response_Send("+ERROR:NOT_CONNECTED\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+DISC: dev=%d, hdl=0x%04X", dev_idx, dev->conn_handle);
    
    /* Start service discovery - results will come async via GATT events */
    ret = BLE_GATT_DiscoverAllServices(dev->conn_handle);
    if (ret != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    /* OK sent immediately, +SERVICE responses will follow after GATT events */
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_INFO_Handler(uint8_t dev_idx)
{
    BLE_Device_t *dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (!dev) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+INFO: dev=%d", dev_idx);
    AT_Response_Send("+INFO:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                   dev->mac_addr[5], dev->mac_addr[4], dev->mac_addr[3],
                   dev->mac_addr[2], dev->mac_addr[1], dev->mac_addr[0]);
    AT_Response_Send("OK\r\n");
    return 0;
}

// ==================== System/Lifecycle Handlers ====================

int AT_RESET_Handler(void)
{
    DEBUG_WARN("AT+RESET");
    AT_Response_Send("OK\r\n");
    
    /* Software reset after delay */
    HAL_Delay(100);
    Module_System_SoftwareReset();
    
    /* Never returns */
    return 0;
}

int AT_HWRESET_Handler(void)
{
    int ret = Module_System_HardwareReset();
    
    if (ret == 0) {
        AT_Response_Send("OK\r\n");
        return 0;
    } else {
        AT_Response_Send("+ERROR:NOT_SUPPORTED\r\n");
        return -1;
    }
}

int AT_FACTORY_Handler(void)
{
    DEBUG_WARN("AT+FACTORY");
    AT_Response_Send("OK\r\n");
    
    /* Factory reset and reboot */
    HAL_Delay(100);
    Module_System_FactoryReset();
    
    /* Never returns */
    return 0;
}

// ==================== Info/Config Handlers ====================

int AT_GETINFO_Handler(void)
{
    char version_buf[64];
    char ble_version_buf[64];
    uint8_t addr_type;
    uint8_t bd_addr[6];
    
    DEBUG_INFO("AT+GETINFO");
    
    /* Get firmware version */
    Module_System_GetVersion(version_buf, sizeof(version_buf));
    AT_Response_Send("+FW:%s\r\n", version_buf);
    
    /* Get BLE stack version */
    Module_System_GetBLEVersion(ble_version_buf, sizeof(ble_version_buf));
    AT_Response_Send("+BLE:%s\r\n", ble_version_buf);
    
    /* Get BD address */
    if (Module_System_GetBDAddr(&addr_type, bd_addr) == 0) {
        AT_Response_Send("+BDADDR:%02X:%02X:%02X:%02X:%02X:%02X\r\n",
                       bd_addr[5], bd_addr[4], bd_addr[3],
                       bd_addr[2], bd_addr[1], bd_addr[0]);
    }
    
    /* Get uptime */
    uint32_t uptime = Module_System_GetUptime();
    AT_Response_Send("+UPTIME:%lu ms\r\n", uptime);
    
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_NAME_Handler(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+NAME: %s", name);
    
    if (Module_Config_SetDeviceName(name) == 0) {
        AT_Response_Send("OK\r\n");
        return 0;
    } else {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
}

int AT_COMM_Handler(uint32_t baud, uint8_t parity, uint8_t stop)
{
    UART_Config_t uart_config;
    
    DEBUG_INFO("AT+COMM: baud=%lu, parity=%d, stop=%d", baud, parity, stop);
    
    uart_config.baud_rate = baud;
    uart_config.parity = parity;
    uart_config.stop_bits = stop;
    uart_config.data_bits = 8;
    
    if (Module_Config_SetUART(&uart_config) == 0) {
        AT_Response_Send("OK\r\n");
        
        /* Apply UART changes after response */
        HAL_Delay(50);
        Module_Config_ApplyUART();
        
        return 0;
    } else {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
}

int AT_RF_Handler(int8_t tx_power, uint16_t scan_interval, uint16_t scan_window)
{
    RF_Config_t rf_config;
    
    DEBUG_INFO("AT+RF: tx=%d dBm, scan_int=%d, scan_win=%d", 
               tx_power, scan_interval, scan_window);
    
    rf_config.tx_power_dbm = tx_power;
    rf_config.scan_interval = scan_interval;
    rf_config.scan_window = scan_window;
    rf_config.conn_interval_min = 0x0018;  /* Keep defaults */
    rf_config.conn_interval_max = 0x0028;
    
    if (Module_Config_SetRF(&rf_config) == 0) {
        /* Apply RF config immediately */
        Module_Config_ApplyRF();
        AT_Response_Send("OK\r\n");
        return 0;
    } else {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
}

int AT_SAVE_Handler(void)
{
    DEBUG_INFO("AT+SAVE");
    
    if (Module_Config_Save() == 0) {
        AT_Response_Send("OK\r\n");
        return 0;
    } else {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
}

// ==================== Mode Handlers ====================

int AT_CMDMODE_Handler(void)
{
    DEBUG_INFO("AT+CMDMODE");
    
    if (Module_Mode_EnterCommand() == 0) {
        AT_Response_Send("OK\r\n");
        return 0;
    } else {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
}

int AT_DATAMODE_Handler(uint8_t dev_idx, uint16_t char_handle)
{
    DEBUG_INFO("AT+DATAMODE: dev=%d, handle=0x%04X", dev_idx, char_handle);
    
    if (Module_Mode_EnterData(dev_idx, char_handle) == 0) {
        AT_Response_Send("OK\r\n");
        return 0;
    } else {
        AT_Response_Send("+ERROR:NOT_CONNECTED\r\n");
        return -1;
    }
}

// ==================== Status Handlers ====================

int AT_STATUS_Handler(uint8_t dev_idx)
{
    uint8_t i, count;
    BLE_Device_t *dev;
    
    DEBUG_INFO("AT+STATUS: idx=%d", dev_idx);
    
    if (dev_idx == 0xFF) {
        /* Report all devices */
        count = BLE_DeviceManager_GetCount();
        AT_Response_Send("+STATUS:%d devices\r\n", (int)count);
        
        for (i = 0; i < count; i++) {
            dev = BLE_DeviceManager_GetDevice((int)i);
            if (dev != NULL) {
                AT_Response_Send("+DEV:%d,%s,0x%04X\r\n",
                    (int)i,
                    dev->is_connected ? "CONNECTED" : "DISCONNECTED",
                    dev->conn_handle);
            }
        }
    } else {
        /* Report specific device */
        dev = BLE_DeviceManager_GetDevice(dev_idx);
        if (dev == NULL) {
            AT_Response_Send("ERROR\r\n");
            return -1;
        }
        
        AT_Response_Send("+STATUS:%s,0x%04X,RSSI=%d\r\n",
            dev->is_connected ? "CONNECTED" : "DISCONNECTED",
            dev->conn_handle,
            (int)dev->rssi);
    }
    
    AT_Response_Send("OK\r\n");
    return 0;
}

// ==================== Power Management Handlers ====================

int AT_SLEEP_Handler(uint8_t mode, uint8_t wake_mask, uint32_t timeout_ms)
{
    Power_Mode_t power_mode;
    
    DEBUG_INFO("AT+SLEEP: mode=%d, wake=0x%02X, timeout=%lu", 
               mode, wake_mask, timeout_ms);
    
    /* Validate mode */
    if (mode < 1 || mode > 4) {
        AT_Response_Send("+ERROR:INVALID_MODE\r\n");
        return -1;
    }
    
    power_mode = (Power_Mode_t)mode;
    
    AT_Response_Send("OK\r\n");
    HAL_Delay(50);  /* Allow UART transmission to complete */
    
    /* Enter sleep */
    Module_Power_EnterSleep(power_mode, wake_mask, timeout_ms);
    
    /* After wake - send wake event */
    AT_Response_Send("+WAKE\r\n");
    
    return 0;
}

int AT_WAKE_Handler(void)
{
    /* Manual wake command (always succeeds in run mode) */
    DEBUG_INFO("AT+WAKE");
    AT_Response_Send("OK\r\n");
    return 0;
}

// ==================== Diagnostics Handlers ====================

int AT_DIAG_Handler(uint8_t dev_idx)
{
    BLE_Device_t *dev;
    
    DEBUG_INFO("AT+DIAG: dev=%d", dev_idx);
    
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    /* Report diagnostics */
    AT_Response_Send("+DIAG:RSSI=%d dBm\r\n", (int)dev->rssi);
    
    if (dev->is_connected) {
        AT_Response_Send("+DIAG:CONN_HANDLE=0x%04X\r\n", dev->conn_handle);
        AT_Response_Send("+DIAG:STATUS=CONNECTED\r\n");
    } else {
        AT_Response_Send("+DIAG:STATUS=DISCONNECTED\r\n");
    }
    
    /* Add more diagnostic info as needed */
    const RF_Config_t *rf_config = &(Module_Config_Get()->rf);
    AT_Response_Send("+DIAG:TX_POWER=%d dBm\r\n", (int)rf_config->tx_power_dbm);
    
    AT_Response_Send("OK\r\n");
    return 0;
}
