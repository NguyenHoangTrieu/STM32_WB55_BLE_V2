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
#include "circular_buffer.h"
#include "app_conf.h"
#include "stm32_seq.h"

static void Module_AT_Task(void);

void module_ble_init(void)
{
    DEBUG_INFO("=== BLE Gateway Initialization ===");
    
    // Initialize all modules
    BLE_DeviceManager_Init();
    AT_Command_Init();
    BLE_Connection_Init();
    BLE_GATT_Init();
    BLE_EventHandler_Init();

    // Register sequencer task for AT command processing
    UTIL_SEQ_RegTask(1 << CFG_TASK_AT_CMD_PROC_ID, UTIL_SEQ_RFU, Module_AT_Task);
    
    // Register event callbacks
    BLE_EventHandler_RegisterScanCallback(BLE_Connection_OnScanReport);
    BLE_EventHandler_RegisterConnectionCallback(BLE_Connection_OnConnected);
    BLE_EventHandler_RegisterDisconnectionCallback(BLE_Connection_OnDisconnected);
    
    DEBUG_INFO("=== BLE Gateway Ready ===");
}

// Sequencer task: drain AT command buffer and process each complete line
static void Module_AT_Task(void)
{
    CircularBuffer_t *buffer = AT_Command_GetBuffer();
    char cmd_line[128];

    while (CircularBuffer_GetLine(buffer, cmd_line, sizeof(cmd_line)) > 0) {
        AT_Command_Process(cmd_line);
    }
}
