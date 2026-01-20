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

void BLE_EventHandler_Init(void)
{
    scan_cb = NULL;
    conn_cb = NULL;
    disconn_cb = NULL;
    notif_cb = NULL;
    read_cb = NULL;
    write_cb = NULL;
    
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
