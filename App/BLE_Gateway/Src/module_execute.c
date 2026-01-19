/**
  ******************************************************************************
  * @file    module_execute.c
  * @brief   BLE Gateway Application Executor - Implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "module_execute.h"
#include "at_command.h"
#include "ble_device_manager.h"
#include "ble_connection.h"
#include "ble_gatt_client.h"
#include "ble_event_handler.h"
#include "debug_trace.h"
#include "app_conf.h"
#include "stm32_seq.h"

static void Module_AT_Task(void);

/*============================================================================
 * GATT Event Callbacks - Forward to AT Response
 *============================================================================*/

/**
 * @brief Callback for GATT notification received
 */
static void Module_OnNotification(uint16_t conn_handle, uint16_t handle,
                                  const uint8_t *data, uint16_t len)
{
    uint16_t i;
    
    /* Send notification data as hex string via AT response */
    AT_Response_Send("+NOTIFY:0x%04X,0x%04X,", conn_handle, handle);
    
    for (i = 0; i < len; i++) {
        AT_Response_Send("%02X", data[i]);
    }
    AT_Response_Send("\r\n");
}

/**
 * @brief Callback for GATT read response received
 */
static void Module_OnReadResponse(uint16_t conn_handle, uint16_t handle,
                                  const uint8_t *data, uint16_t len)
{
    uint16_t i;
    
    /* Send read data as hex string via AT response */
    AT_Response_Send("+READ:0x%04X,0x%04X,", conn_handle, handle);
    
    for (i = 0; i < len; i++) {
        AT_Response_Send("%02X", data[i]);
    }
    AT_Response_Send("\r\n");
}

/**
 * @brief Callback for GATT write response received
 */
static void Module_OnWriteResponse(uint16_t conn_handle, uint8_t status)
{
    if (status == 0) {
        AT_Response_Send("+WRITE_DONE:0x%04X\r\n", conn_handle);
    } else {
        AT_Response_Send("+WRITE_ERROR:0x%04X,0x%02X\r\n", conn_handle, status);
    }
}

/*============================================================================
 * Module Initialization
 *============================================================================*/

void module_ble_init(void)
{
    DEBUG_INFO("=== BLE Gateway Initialization ===");
    
    /* Initialize all modules */
    BLE_DeviceManager_Init();
    AT_Command_Init();
    BLE_Connection_Init();
    BLE_GATT_Init();
    BLE_EventHandler_Init();

    /* Register sequencer task for AT command processing */
    UTIL_SEQ_RegTask(1 << CFG_TASK_AT_CMD_PROC_ID, UTIL_SEQ_RFU, Module_AT_Task);
    
    /* Register BLE event callbacks */
    BLE_EventHandler_RegisterScanCallback(BLE_Connection_OnScanReport);
    BLE_EventHandler_RegisterConnectionCallback(BLE_Connection_OnConnected);
    BLE_EventHandler_RegisterDisconnectionCallback(BLE_Connection_OnDisconnected);
    
    /* Register GATT event callbacks for AT response */
    BLE_EventHandler_RegisterNotificationCallback(Module_OnNotification);
    BLE_EventHandler_RegisterReadResponseCallback(Module_OnReadResponse);
    BLE_EventHandler_RegisterWriteResponseCallback(Module_OnWriteResponse);
    
    DEBUG_INFO("=== BLE Gateway Ready ===");
}

/* Sequencer task: process AT commands */
static void Module_AT_Task(void)
{
    AT_Command_ProcessReady();
}
