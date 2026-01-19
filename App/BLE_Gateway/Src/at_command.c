/**
  ******************************************************************************
  * @file    at_command.c
  * @brief   AT Command Parser implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "at_command.h"
#include "ble_device_manager.h"
#include "debug_trace.h"
#include "main.h"
#include "app_conf.h"
#include "stm32_seq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

extern UART_HandleTypeDef hlpuart1;

static uint8_t at_buffer[AT_CMD_BUFFER_SIZE];
static CircularBuffer_t at_circular_buf;

// Parse MAC string "AA:BB:CC:DD:EE:FF" to bytes
static int ParseMACString(const char *mac_str, uint8_t *mac_bytes)
{
    if (!mac_str || !mac_bytes) return -1;
    
    int count = 0;
    for (int i = 0; i < 6; i++) {
        unsigned int byte;
        if (sscanf(mac_str + (i * 3), "%x", &byte) != 1) {
            return -1;
        }
        mac_bytes[i] = (uint8_t)byte;
        count++;
    }
    
    return count == 6 ? 0 : -1;
}

void AT_Command_Init(void)
{
    CircularBuffer_Init(&at_circular_buf, at_buffer, AT_CMD_BUFFER_SIZE);
    DEBUG_INFO("AT Command initialized");
}

void AT_Command_ReceiveByte(uint8_t byte)
{
    CircularBuffer_Put(&at_circular_buf, byte);

    // Trigger sequencer task when a line terminator arrives
    if (byte == '\n' || byte == '\r') {
        UTIL_SEQ_SetTask(1 << CFG_TASK_AT_CMD_PROC_ID, CFG_SCH_PRIO_0);
    }
}

CircularBuffer_t* AT_Command_GetBuffer(void)
{
    return &at_circular_buf;
}

void AT_Response_Send(const char *fmt, ...)
{
    static char response_buf[AT_CMD_MAX_LEN];
    va_list args;
    
    va_start(args, fmt);
    vsnprintf(response_buf, AT_CMD_MAX_LEN, fmt, args);
    va_end(args);
    
    // Send via UART (NO printf here!)
    HAL_UART_Transmit(&hlpuart1, (uint8_t *)response_buf, strlen(response_buf), HAL_MAX_DELAY);
}

void AT_Command_Process(const char *cmd_line)
{
    if (!cmd_line) return;
    
    // Remove trailing \r\n
    char cmd[AT_CMD_MAX_LEN];
    strncpy(cmd, cmd_line, AT_CMD_MAX_LEN - 1);
    cmd[AT_CMD_MAX_LEN - 1] = '\0';
    
    // Remove \r\n
    int len = strlen(cmd);
    while (len > 0 && (cmd[len-1] == '\r' || cmd[len-1] == '\n')) {
        cmd[--len] = '\0';
    }
    
    // Debug: Print received command to USB CDC
    DEBUG_PRINT("AT RX: %s", cmd);
    
    // Parse commands
    if (strcmp(cmd, "AT") == 0) {
        AT_Response_Send("OK\r\n");
    }
    else if (strncmp(cmd, "AT+SCAN", 7) == 0) {
        uint16_t duration = 5000;  // Default 5 sec
        int ret = sscanf(cmd + 7, "=%hu", &duration);
        AT_SCAN_Handler(ret == 1 ? duration : 5000);
    }
    else if (strncmp(cmd, "AT+LIST", 7) == 0) {
        AT_LIST_Handler();
    }
    else if (strncmp(cmd, "AT+CONNECT=", 11) == 0) {
        AT_CONNECT_Handler(cmd + 11);
    }
    else if (strncmp(cmd, "AT+DISCONNECT=", 14) == 0) {
        uint8_t idx = (uint8_t)atoi(cmd + 14);
        AT_DISCONNECT_Handler(idx);
    }
    else if (strncmp(cmd, "AT+READ=", 8) == 0) {
        uint8_t idx;
        uint16_t handle;
        if (sscanf(cmd + 8, "%hhu,%hu", &idx, &handle) == 2) {
            AT_READ_Handler(idx, handle);
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+WRITE=", 9) == 0) {
        uint8_t idx;
        uint16_t handle;
        char data_str[64];
        if (sscanf(cmd + 9, "%hhu,%hu,%63s", &idx, &handle, data_str) == 3) {
            AT_WRITE_Handler(idx, handle, data_str);
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+NOTIFY=", 10) == 0) {
        uint8_t idx, enable;
        uint16_t handle;
        if (sscanf(cmd + 10, "%hhu,%hu,%hhu", &idx, &handle, &enable) == 3) {
            AT_NOTIFY_Handler(idx, handle, enable);
        } else {
            AT_Response_Send("ERROR\r\n");
        }
    }
    else if (strncmp(cmd, "AT+DISC=", 8) == 0) {
        uint8_t idx = (uint8_t)atoi(cmd + 8);
        AT_DISC_Handler(idx);
    }
    else if (strncmp(cmd, "AT+INFO=", 8) == 0) {
        uint8_t idx = (uint8_t)atoi(cmd + 8);
        AT_INFO_Handler(idx);
    }
    else {
        AT_Response_Send("ERROR\r\n");
    }
}

// ==================== AT Handlers ====================

int AT_SCAN_Handler(uint16_t duration_ms)
{
    DEBUG_INFO("AT+SCAN: duration=%dms", duration_ms);
    BLE_DeviceManager_SetScanActive(1);
    AT_Response_Send("OK\r\n");
    
    // TODO: Call BLE_Connection_StartScan(duration_ms)
    return 0;
}

int AT_CONNECT_Handler(const char *mac_str)
{
    uint8_t mac[6];
    
    if (ParseMACString(mac_str, mac) != 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    int dev_idx = BLE_DeviceManager_FindDevice(mac);
    if (dev_idx < 0) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+CONNECT: device %d", dev_idx);
    AT_Response_Send("OK\r\n");
    
    // TODO: Call BLE_Connection_CreateConnection(mac)
    return 0;
}

int AT_DISCONNECT_Handler(uint8_t dev_idx)
{
    BLE_Device_t *dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (!dev || !dev->is_connected) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+DISCONNECT: device %d", dev_idx);
    AT_Response_Send("OK\r\n");
    
    // TODO: Call BLE_Connection_TerminateConnection(dev->conn_handle)
    return 0;
}

int AT_LIST_Handler(void)
{
    DEBUG_INFO("AT+LIST");
    
    uint8_t count = BLE_DeviceManager_GetCount();
    AT_Response_Send("+LIST:%d\r\n", count);
    
    for (int i = 0; i < count; i++) {
        BLE_Device_t *dev = BLE_DeviceManager_GetDevice(i);
        if (dev) {
            AT_Response_Send("+DEV:%d,%02X:%02X:%02X:%02X:%02X:%02X,%d,%04X\r\n",
                           i,
                           dev->mac_addr[5], dev->mac_addr[4], dev->mac_addr[3],
                           dev->mac_addr[2], dev->mac_addr[1], dev->mac_addr[0],
                           dev->rssi,
                           dev->conn_handle);
        }
    }
    
    AT_Response_Send("OK\r\n");
    return 0;
}

int AT_READ_Handler(uint8_t dev_idx, uint16_t char_handle)
{
    BLE_Device_t *dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (!dev || !dev->is_connected) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+READ: dev=%d, handle=%04X", dev_idx, char_handle);
    AT_Response_Send("OK\r\n");
    
    // TODO: Call BLE_GATT_ReadCharacteristic(dev->conn_handle, char_handle)
    return 0;
}

int AT_WRITE_Handler(uint8_t dev_idx, uint16_t char_handle, const char *data)
{
    BLE_Device_t *dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (!dev || !dev->is_connected) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+WRITE: dev=%d, handle=%04X, data=%s", dev_idx, char_handle, data);
    AT_Response_Send("OK\r\n");
    
    // TODO: Convert data_str hex to bytes, call BLE_GATT_WriteCharacteristic
    return 0;
}

int AT_NOTIFY_Handler(uint8_t dev_idx, uint16_t desc_handle, uint8_t enable)
{
    BLE_Device_t *dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (!dev || !dev->is_connected) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+NOTIFY: dev=%d, handle=%04X, enable=%d", dev_idx, desc_handle, enable);
    AT_Response_Send("OK\r\n");
    
    // TODO: Call BLE_GATT_EnableNotification or BLE_GATT_DisableNotification
    return 0;
}

int AT_DISC_Handler(uint8_t dev_idx)
{
    BLE_Device_t *dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (!dev || !dev->is_connected) {
        AT_Response_Send("ERROR\r\n");
        return -1;
    }
    
    DEBUG_INFO("AT+DISC: dev=%d", dev_idx);
    AT_Response_Send("OK\r\n");
    
    // TODO: Call BLE_GATT_DiscoverAllServices(dev->conn_handle)
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
