/**
  ******************************************************************************
  * @file    ble_connection.c
  * @brief   BLE Connection Management implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "ble_connection.h"
#include "ble_device_manager.h"
#include "debug_trace.h"
#include "at_command.h"
#include "ble_hci_le.h"

extern void AT_Response_Send(const char *fmt, ...);

typedef struct {
    uint16_t conn_handle;
    BLE_ConnectionState_t state;
    uint8_t mac_addr[BLE_MAC_LEN];
} ConnectionInfo_t;

static ConnectionInfo_t connections[MAX_BLE_CONNECTIONS];
static uint8_t connection_count = 0;

void BLE_Connection_Init(void)
{
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        connections[i].conn_handle = 0xFFFF;
        connections[i].state = CONN_STATE_IDLE;
    }
    connection_count = 0;
    DEBUG_INFO("Connection Manager initialized");
}

int BLE_Connection_StartScan(uint16_t duration_ms)
{
    DEBUG_INFO("Starting BLE scan: %dms", duration_ms);
    
    // Set scan parameters: Active scan, 10ms interval/window
    uint8_t ret = hci_le_set_scan_parameters(
        0x01,           // Active scan
        0x0010,         // Scan interval: 10ms (0x0010 * 0.625ms)
        0x0010,         // Scan window: 10ms
        0x01,           // Public address type
        0x00            // Accept all devices
    );
    
    if (ret != 0) {
        DEBUG_ERROR("Failed to set scan parameters: 0x%02X", ret);
        return -1;
    }
    
    // Enable scan
    ret = hci_le_set_scan_enable(
        0x01,           // Enable scan
        0x00            // No duplicate filtering (report all)
    );
    
    if (ret != 0) {
        DEBUG_ERROR("Failed to enable scan: 0x%02X", ret);
        return -1;
    }
    
    BLE_DeviceManager_SetScanActive(1);
    return 0;
}

int BLE_Connection_StopScan(void)
{
    DEBUG_INFO("Stopping BLE scan");
    
    uint8_t ret = hci_le_set_scan_enable(0x00, 0x00);  // Disable scan
    
    if (ret != 0) {
        DEBUG_ERROR("Failed to disable scan: 0x%02X", ret);
        return -1;
    }
    
    BLE_DeviceManager_SetScanActive(0);
    return 0;
}

int BLE_Connection_CreateConnection(const uint8_t *mac)
{
    if (!mac) return -1;
    
    DEBUG_INFO("Creating connection to device");
    DEBUG_PrintMAC(mac);
    
    // Stop scan first
    BLE_Connection_StopScan();
    
    // Create connection
    uint8_t ret = hci_le_create_connection(
        0x0010,         // Scan interval: 10ms
        0x0010,         // Scan window: 10ms
        0x00,           // Initiator filter policy: no whitelist
        0x01,           // Peer address type: public
        (uint8_t *)mac, // Peer address
        0x01,           // Own address type: public
        0x0018,         // Connection interval min: 24 * 1.25ms = 30ms
        0x0028,         // Connection interval max: 40 * 1.25ms = 50ms
        0x0000,         // Connection latency: 0
        0x00C8,         // Supervision timeout: 200 * 10ms = 2s
        0x0000,         // Min CE length: 0
        0x0000          // Max CE length: 0
    );
    
    if (ret != 0) {
        DEBUG_ERROR("Failed to create connection: 0x%02X", ret);
        return -1;
    }
    
    AT_Response_Send("+CONNECTING\r\n");
    return 0;
}

int BLE_Connection_TerminateConnection(uint16_t conn_handle)
{
    DEBUG_INFO("Terminating connection: 0x%04X", conn_handle);
    
    uint8_t ret = hci_disconnect(conn_handle, 0x13);  // 0x13 = User terminated
    
    if (ret != 0) {
        DEBUG_ERROR("Failed to terminate connection: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

void BLE_Connection_SetState(uint16_t conn_handle, BLE_ConnectionState_t state)
{
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            connections[i].state = state;
            DEBUG_INFO("Connection 0x%04X state: %d", conn_handle, state);
            return;
        }
    }
}

BLE_ConnectionState_t BLE_Connection_GetState(uint16_t conn_handle)
{
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            return connections[i].state;
        }
    }
    return CONN_STATE_IDLE;
}

uint8_t BLE_Connection_IsConnected(uint16_t conn_handle)
{
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            return (connections[i].state == CONN_STATE_CONNECTED) ? 1 : 0;
        }
    }
    return 0;
}

void BLE_Connection_OnScanReport(const uint8_t *mac, int8_t rssi)
{
    if (!mac) return;
    
    int idx = BLE_DeviceManager_AddDevice(mac, rssi);
    
    if (idx >= 0) {
        // Send scan report to UART
        AT_Response_Send("+SCAN:%02X:%02X:%02X:%02X:%02X:%02X,%d\r\n",
                       mac[5], mac[4], mac[3], mac[2], mac[1], mac[0], rssi);
    }
}

void BLE_Connection_OnConnected(const uint8_t *mac, uint16_t conn_handle, uint8_t status)
{
    DEBUG_INFO("Connection complete: handle=0x%04X, status=0x%02X", conn_handle, status);
    
    if (status != 0) {
        DEBUG_ERROR("Connection failed with status 0x%02X", status);
        AT_Response_Send("+CONN_ERROR:%02X\r\n", status);
        return;
    }
    
    // Find device and update
    int dev_idx = BLE_DeviceManager_FindDevice(mac);
    if (dev_idx >= 0) {
        BLE_DeviceManager_UpdateConnection(dev_idx, conn_handle, 1);
        
        // Store connection
        for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
            if (connections[i].conn_handle == 0xFFFF) {
                connections[i].conn_handle = conn_handle;
                connections[i].state = CONN_STATE_CONNECTED;
                memcpy(connections[i].mac_addr, mac, BLE_MAC_LEN);
                connection_count++;
                break;
            }
        }
        
        AT_Response_Send("+CONNECTED:%d,0x%04X\r\n", dev_idx, conn_handle);
    }
}

void BLE_Connection_OnDisconnected(uint16_t conn_handle, uint8_t reason)
{
    DEBUG_INFO("Disconnected: handle=0x%04X, reason=0x%02X", conn_handle, reason);
    
    // Find and update device
    int dev_idx = BLE_DeviceManager_FindConnHandle(conn_handle);
    if (dev_idx >= 0) {
        BLE_DeviceManager_UpdateConnection(dev_idx, conn_handle, 0);
    }
    
    // Remove from connections
    for (int i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            connections[i].conn_handle = 0xFFFF;
            connections[i].state = CONN_STATE_IDLE;
            if (connection_count > 0) connection_count--;
            break;
        }
    }
    
    AT_Response_Send("+DISCONNECTED:0x%04X\r\n", conn_handle);
}
