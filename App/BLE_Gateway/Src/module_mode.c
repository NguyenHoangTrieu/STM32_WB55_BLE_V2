/**
  ******************************************************************************
  * @file    module_mode.c
  * @brief   Mode Control Implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "module_mode.h"
#include "debug_trace.h"
#include "main.h"
#include "ble_device_manager.h"
#include "ble_gatt_client.h"
#include "at_command.h"
#include <string.h>

/* Current mode state */
static Operation_Mode_t current_mode = MODE_COMMAND;
static uint8_t target_dev_idx = 0xFF;
static uint16_t target_char_handle = 0;

/* Data mode TX buffer */
#define DATA_TX_BUFFER_SIZE  512
static uint8_t data_tx_buffer[DATA_TX_BUFFER_SIZE];
static uint16_t data_tx_len = 0;

/* Escape sequence detection */
static uint8_t escape_count = 0;
static uint32_t last_char_time = 0;
static uint32_t escape_start_time = 0;
static uint8_t escape_detected = 0;

/* External handles */
extern UART_HandleTypeDef hlpuart1;

/*============================================================================
 * Initialization
 *============================================================================*/
void Module_Mode_Init(void)
{
    current_mode = MODE_COMMAND;
    target_dev_idx = 0xFF;
    target_char_handle = 0;
    data_tx_len = 0;
    escape_count = 0;
    escape_detected = 0;
    
    DEBUG_INFO("Mode control initialized");
}

/*============================================================================
 * Mode Switching
 *============================================================================*/
int Module_Mode_EnterCommand(void)
{
    if (current_mode == MODE_COMMAND) {
        return 0;  /* Already in command mode */
    }
    
    DEBUG_INFO("Entering command mode");
    
    /* Flush any pending data */
    Module_Mode_FlushTxBuffer();
    
    /* Switch to command mode */
    current_mode = MODE_COMMAND;
    target_dev_idx = 0xFF;
    target_char_handle = 0;
    escape_count = 0;
    escape_detected = 0;
    
    /* Send confirmation */
    AT_Response_Send("+CMDMODE\r\n");
    
    return 0;
}

int Module_Mode_EnterData(uint8_t dev_idx, uint16_t char_handle)
{
    BLE_Device_t *dev;
    
    /* Validate device */
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL || !dev->is_connected) {
        DEBUG_ERROR("Cannot enter data mode: device not connected");
        return -1;
    }
    
    DEBUG_INFO("Entering data mode: dev=%d, handle=0x%04X", dev_idx, char_handle);
    
    /* Switch to data mode */
    current_mode = MODE_DATA;
    target_dev_idx = dev_idx;
    target_char_handle = char_handle;
    data_tx_len = 0;
    escape_count = 0;
    escape_detected = 0;
    last_char_time = HAL_GetTick();
    
    /* Send confirmation */
    AT_Response_Send("+DATAMODE\r\n");
    
    return 0;
}

Operation_Mode_t Module_Mode_GetCurrent(void)
{
    return current_mode;
}

/*============================================================================
 * Data Mode Processing
 *============================================================================*/
void Module_Mode_ProcessDataByte(uint8_t byte)
{
    uint32_t current_time = HAL_GetTick();
    uint32_t time_since_last = current_time - last_char_time;
    BLE_Device_t *dev;
    
    /* Check for escape sequence: +++ with guard time */
    if (byte == ESCAPE_SEQ_CHAR) {
        if (escape_count == 0) {
            /* First + */
            if (time_since_last >= ESCAPE_GUARD_TIME_MS) {
                escape_start_time = current_time;
                escape_count = 1;
            }
        } else if (escape_count < ESCAPE_SEQ_LENGTH) {
            /* Subsequent + */
            escape_count++;
            
            if (escape_count == ESCAPE_SEQ_LENGTH) {
                /* Complete escape sequence - check trailing guard time later */
                escape_detected = 1;
                /* Don't add to buffer */
                last_char_time = current_time;
                return;
            }
        }
    } else {
        /* Non-+ character resets escape detection if incomplete */
        if (escape_count > 0 && escape_count < ESCAPE_SEQ_LENGTH) {
            /* Add the buffered + characters */
            for (uint8_t i = 0; i < escape_count && data_tx_len < DATA_TX_BUFFER_SIZE; i++) {
                data_tx_buffer[data_tx_len++] = ESCAPE_SEQ_CHAR;
            }
        }
        escape_count = 0;
    }
    
    /* Add byte to TX buffer */
    if (data_tx_len < DATA_TX_BUFFER_SIZE) {
        data_tx_buffer[data_tx_len++] = byte;
    } else {
        /* Buffer full - flush it */
        Module_Mode_FlushTxBuffer();
        data_tx_buffer[data_tx_len++] = byte;
    }
    
    last_char_time = current_time;
    
    /* Auto-flush if buffer reaches threshold or timeout */
    if (data_tx_len >= (DATA_TX_BUFFER_SIZE - 20) || time_since_last > 10) {
        Module_Mode_FlushTxBuffer();
    }
}

void Module_Mode_ProcessGATTData(uint16_t conn_handle, uint16_t handle, 
                                 const uint8_t *data, uint16_t len)
{
    BLE_Device_t *dev;
    
    /* Only forward if in data mode and target matches */
    if (current_mode != MODE_DATA) {
        return;
    }
    
    dev = BLE_DeviceManager_GetDevice(target_dev_idx);
    if (dev == NULL || dev->conn_handle != conn_handle) {
        return;
    }
    
    if (handle != target_char_handle) {
        return;
    }
    
    /* Forward data to UART */
    HAL_UART_Transmit(&hlpuart1, (uint8_t*)data, len, 100);
}

uint8_t Module_Mode_IsEscapeDetected(void)
{
    uint32_t current_time = HAL_GetTick();
    
    if (escape_detected) {
        /* Check trailing guard time */
        if ((current_time - last_char_time) >= ESCAPE_GUARD_TIME_MS) {
            /* Valid escape sequence with trailing guard time */
            escape_detected = 0;
            escape_count = 0;
            return 1;
        }
    }
    
    return 0;
}

uint8_t Module_Mode_GetTargetDevice(void)
{
    return target_dev_idx;
}

uint16_t Module_Mode_GetTargetHandle(void)
{
    return target_char_handle;
}

int Module_Mode_FlushTxBuffer(void)
{
    BLE_Device_t *dev;
    int ret;
    
    if (data_tx_len == 0) {
        return 0;
    }
    
    if (current_mode != MODE_DATA) {
        data_tx_len = 0;
        return 0;
    }
    
    /* Get target device */
    dev = BLE_DeviceManager_GetDevice(target_dev_idx);
    if (dev == NULL || !dev->is_connected) {
        DEBUG_ERROR("Data mode target disconnected");
        data_tx_len = 0;
        /* Auto-exit data mode on disconnect */
        Module_Mode_EnterCommand();
        return -1;
    }
    
    /* Write to GATT characteristic */
    ret = BLE_GATT_WriteCharacteristic(dev->conn_handle, target_char_handle, 
                                       data_tx_buffer, data_tx_len);
    
    if (ret == 0) {
        DEBUG_PRINT("Data TX: %d bytes", data_tx_len);
        data_tx_len = 0;
        return data_tx_len;
    } else {
        DEBUG_ERROR("Data TX failed");
        data_tx_len = 0;
        return -1;
    }
}
