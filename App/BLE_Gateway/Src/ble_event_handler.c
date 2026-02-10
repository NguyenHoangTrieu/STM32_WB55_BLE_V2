/**
  ******************************************************************************
  * @file    ble_event_handler.c
  * @brief   BLE Event Handler implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "ble_event_handler.h"
#include "debug_trace.h"

// Event callbacks
static BLE_ScanReportCallback_t scan_cb = NULL;
static BLE_ConnectionCompleteCallback_t conn_cb = NULL;
static BLE_DisconnectionCompleteCallback_t disconn_cb = NULL;
static BLE_GATTCNotificationCallback_t notif_cb = NULL;
static BLE_GATTCReadResponseCallback_t read_cb = NULL;
static BLE_GATTCWriteResponseCallback_t write_cb = NULL;
static BLE_GATTCProcCompleteCallback_t proc_complete_cb = NULL;

void BLE_EventHandler_Init(void)
{
    scan_cb = NULL;
    conn_cb = NULL;
    disconn_cb = NULL;
    notif_cb = NULL;
    read_cb = NULL;
    write_cb = NULL;
    proc_complete_cb = NULL;
    
    DEBUG_INFO("Event Handler initialized");
}

void BLE_EventHandler_RegisterScanCallback(BLE_ScanReportCallback_t cb)
{
    scan_cb = cb;
}

void BLE_EventHandler_RegisterConnectionCallback(BLE_ConnectionCompleteCallback_t cb)
{
    conn_cb = cb;
}

void BLE_EventHandler_RegisterDisconnectionCallback(BLE_DisconnectionCompleteCallback_t cb)
{
    disconn_cb = cb;
}

void BLE_EventHandler_RegisterNotificationCallback(BLE_GATTCNotificationCallback_t cb)
{
    notif_cb = cb;
}

void BLE_EventHandler_RegisterReadResponseCallback(BLE_GATTCReadResponseCallback_t cb)
{
    read_cb = cb;
}

void BLE_EventHandler_RegisterWriteResponseCallback(BLE_GATTCWriteResponseCallback_t cb)
{
    write_cb = cb;
}

void BLE_EventHandler_RegisterGattProcCompleteCallback(BLE_GATTCProcCompleteCallback_t cb)
{
    proc_complete_cb = cb;
}

void BLE_EventHandler_OnScanReport(const uint8_t *mac, int8_t rssi, const char *name, uint8_t addr_type)
{
    DEBUG_PRINT("Event: Scan Report - RSSI=%d", rssi);
    if (scan_cb) {
        scan_cb(mac, rssi, name, addr_type);
    }
}


void BLE_EventHandler_OnConnectionComplete(const uint8_t *mac, uint16_t conn_handle, uint8_t status)
{
    DEBUG_PRINT("Event: Connection Complete - handle=0x%04X, status=0x%02X", conn_handle, status);
    if (conn_cb) {
        conn_cb(mac, conn_handle, status);
    }
}

void BLE_EventHandler_OnDisconnectionComplete(uint16_t conn_handle, uint8_t reason)
{
    DEBUG_PRINT("Event: Disconnection Complete - handle=0x%04X, reason=0x%02X", conn_handle, reason);
    if (disconn_cb) {
        disconn_cb(conn_handle, reason);
    }
}

void BLE_EventHandler_OnNotification(uint16_t conn_handle, uint16_t handle,
                                      const uint8_t *data, uint16_t len)
{
    DEBUG_PRINT("Event: Notification - conn=0x%04X, handle=0x%04X, len=%d", conn_handle, handle, len);
    if (notif_cb) {
        notif_cb(conn_handle, handle, data, len);
    }
}

void BLE_EventHandler_OnReadResponse(uint16_t conn_handle, uint16_t handle,
                                      const uint8_t *data, uint16_t len)
{
    DEBUG_PRINT("Event: Read Response - conn=0x%04X, handle=0x%04X, len=%d", conn_handle, handle, len);
    if (read_cb) {
        read_cb(conn_handle, handle, data, len);
    }
}

void BLE_EventHandler_OnWriteResponse(uint16_t conn_handle, uint8_t status)
{
    DEBUG_PRINT("Event: Write Response - conn=0x%04X, status=0x%02X", conn_handle, status);
    if (write_cb) {
        write_cb(conn_handle, status);
    }
}

void BLE_EventHandler_OnGattProcComplete(uint16_t conn_handle, uint8_t error_code)
{
    DEBUG_PRINT("Event: GATT Proc Complete - conn=0x%04X, error=0x%02X", conn_handle, error_code);
    if (proc_complete_cb) {
        proc_complete_cb(conn_handle, error_code);
    }
}

void BLE_EventHandler_OnServiceDiscovered(uint16_t conn_handle, const uint8_t *data,
                                           uint16_t data_len, uint8_t attr_data_len)
{
    extern void AT_Response_Send(const char *fmt, ...);
    
    /* Parse and send service discovery results
     * Data format: [start_handle(2), end_handle(2), UUID(2 or 16)]
     */
    uint8_t num_services = data_len / attr_data_len;
    uint8_t i;
    uint16_t start_handle, end_handle, uuid16;
    
    DEBUG_PRINT("Event: Service Discovered - conn=0x%04X, services=%d",
                conn_handle, num_services);
    
    for (i = 0; i < num_services; i++) {
        uint8_t offset = i * attr_data_len;
        
        /* Extract handles */
        start_handle = (uint16_t)(data[offset] | (data[offset + 1] << 8));
        end_handle = (uint16_t)(data[offset + 2] | (data[offset + 3] << 8));
        
        /* Parse UUID - check if 16-bit or 128-bit */
        if (attr_data_len == 6) {
            /* 16-bit UUID */
            uuid16 = (uint16_t)(data[offset + 4] | (data[offset + 5] << 8));
            AT_Response_Send("+SERVICE:0x%04X,0x%04X,0x%04X\r\n",
                           start_handle, end_handle, uuid16);
        } else if (attr_data_len == 20) {
            /* 128-bit UUID - send base + 16-bit UUID */
            uuid16 = (uint16_t)(data[offset + 16] | (data[offset + 17] << 8));
            AT_Response_Send("+SERVICE:0x%04X,0x%04X,0x%04X\r\n",
                           start_handle, end_handle, uuid16);
        }
    }
}

void BLE_EventHandler_OnCharacteristicDiscovered(uint16_t conn_handle, const uint8_t *data,
                                                   uint16_t data_len, uint8_t pair_len)
{
    extern void AT_Response_Send(const char *fmt, ...);
    
    /* Parse and send characteristic discovery results
     * Data format: [attr_handle(2), properties(1), value_handle(2), UUID(2 or 16)]
     */
    DEBUG_PRINT("Event: Char Discovered - conn=0x%04X, data_len=%d, pair_len=%d",
                conn_handle, data_len, pair_len);
    
    /* Need at least 1 byte for length + actual data */
    if (data_len < 2 || pair_len < 5) {
        DEBUG_PRINT("Invalid char discovery data");
        return;
    }
    
    /* Adjust data_len (first byte is length, ignore it) */
    uint16_t actual_len = data_len - 1;
    uint8_t num_chars = actual_len / pair_len;
    uint8_t i;
    
    for (i = 0; i < num_chars; i++) {
        uint8_t offset = i * pair_len;
        uint16_t attr_handle, value_handle, uuid16;
        uint8_t properties;
        
        /* Extract fields */
       attr_handle = (uint16_t)(data[offset] | (data[offset + 1] << 8));
        properties = data[offset + 2];
        value_handle = (uint16_t)(data[offset + 3] | (data[offset + 4] << 8));
        
        /* Parse UUID */
        if (pair_len == 7) {
            /* 16-bit UUID */
            uuid16 = (uint16_t)(data[offset + 5] | (data[offset + 6] << 8));
            AT_Response_Send("+CHAR:0x%04X,0x%02X,0x%04X,0x%04X\r\n",
                           attr_handle, properties, value_handle, uuid16);
        } else if (pair_len == 21) {
            /* 128-bit UUID - extract 16-bit portion */
            uuid16 = (uint16_t)(data[offset + 17] | (data[offset + 18] << 8));
            AT_Response_Send("+CHAR:0x%04X,0x%02X,0x%04X,0x%04X\r\n",
                           attr_handle, properties, value_handle, uuid16);
        }
    }
}
