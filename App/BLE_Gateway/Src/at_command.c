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
            AT_Response_Send("+DEV:%d,%02X:%02X:%02X:%02X:%02X:%02X,%d,%04X\r\n",
                           (int)i,
                           dev->mac_addr[5], dev->mac_addr[4], dev->mac_addr[3],
                           dev->mac_addr[2], dev->mac_addr[1], dev->mac_addr[0],
                           (int)dev->rssi,
                           dev->conn_handle);
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
