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
#include "ble_gap_aci.h"
#include "ble_hci_le.h"
#include <string.h>

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
    uint8_t i;
    
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        connections[i].conn_handle = 0xFFFF;
        connections[i].state = CONN_STATE_IDLE;
    }
    connection_count = 0;
    DEBUG_INFO("Connection Manager initialized");
}

int BLE_Connection_StartScan(uint16_t duration_ms)
{
    tBleStatus ret;
    
    DEBUG_INFO("Starting BLE scan: %dms", duration_ms);

    BLE_DeviceManager_ResetScanFlags();
    
    /* Use ACI_GAP_START_GENERAL_DISCOVERY_PROC for active scanning
     * Scan interval: 0x0010 = 10ms
     * Scan window: 0x0010 = 10ms  
     * Own address: Public (0x00)
     * Filter duplicates: No (0x00) - report all devices
     */
    ret = aci_gap_start_general_discovery_proc(
        0x0010,     /* LE_Scan_Interval: 10ms */
        0x0010,     /* LE_Scan_Window: 10ms */
        0x00,       /* Own_Address_Type: Public */
        0x00        /* Filter_Duplicates: Disabled (report all) */
    );
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to start general discovery: 0x%02X", ret);
        return -1;
    }
    
    BLE_DeviceManager_SetScanActive(1);
    DEBUG_INFO("Scan started successfully");
    return 0;
}

int BLE_Connection_StopScan(void)
{
    tBleStatus ret;
    
    DEBUG_INFO("Stopping BLE scan");
    
    /* Terminate general discovery procedure (0x02 = GAP_GENERAL_DISCOVERY_PROC) */
    ret = aci_gap_terminate_gap_proc(0x02);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to terminate scan: 0x%02X", ret);
        return -1;
    }
    
    BLE_DeviceManager_SetScanActive(0);
    DEBUG_INFO("Scan stopped successfully");
    return 0;
}

int BLE_Connection_CreateConnection(const uint8_t *mac)
{
    tBleStatus ret;
    BLE_Device_t *dev;
    int dev_idx;
    
    if (mac == NULL) {
        return -1;
    }
    
    DEBUG_INFO("Creating connection to device");
    DEBUG_PrintMAC(mac);
    
    /* Find device to get addr_type */
    dev_idx = BLE_DeviceManager_FindDevice(mac);
    if (dev_idx < 0) {
        DEBUG_ERROR("Device not found in list");
        return -1;
    }
    
    dev = BLE_DeviceManager_GetDevice(dev_idx);
    if (dev == NULL) {
        return -1;
    }
    
    /* Stop scan first */
    BLE_Connection_StopScan();
    
    /* Create connection using ACI_GAP_CREATE_CONNECTION
     * Peer address type: 0x00 = Public (most common)
     */
    ret = aci_gap_create_connection(
        0x0010,         /* LE_Scan_Interval: 10ms (0x0010 * 0.625ms) */
        0x0010,         /* LE_Scan_Window: 10ms */
        dev->addr_type, /* Peer_Address_Type: Public */
        mac,            /* Peer_Address */
        0x00,           /* Own_Address_Type: Public */
        0x0018,         /* Conn_Interval_Min: 30ms (24 * 1.25ms) */
        0x0028,         /* Conn_Interval_Max: 50ms (40 * 1.25ms) */
        0x0000,         /* Conn_Latency: 0 */
        0x00C8,         /* Supervision_Timeout: 2000ms (200 * 10ms) */
        0x0000,         /* Minimum_CE_Length: 0 */
        0x0000          /* Maximum_CE_Length: 0 */
    );
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to create connection: 0x%02X", ret);
        return -1;
    }
    
    AT_Response_Send("+CONNECTING\r\n");
    DEBUG_INFO("Connection initiated");
    return 0;
}

int BLE_Connection_TerminateConnection(uint16_t conn_handle)
{
    tBleStatus ret;
    
    DEBUG_INFO("Terminating connection: 0x%04X", conn_handle);
    
    /* Use HCI_DISCONNECT command - available in ble_hci_le.h
     * Reason: 0x13 = Remote User Terminated Connection
     */
    ret = hci_disconnect(conn_handle, 0x13);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to terminate connection: 0x%02X", ret);
        return -1;
    }
    
    DEBUG_INFO("Disconnect initiated");
    return 0;
}

void BLE_Connection_SetState(uint16_t conn_handle, BLE_ConnectionState_t state)
{
    uint8_t i;
    
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            connections[i].state = state;
            DEBUG_INFO("Conn 0x%04X state: %d", conn_handle, (int)state);
            return;
        }
    }
}

BLE_ConnectionState_t BLE_Connection_GetState(uint16_t conn_handle)
{
    uint8_t i;
    
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            return connections[i].state;
        }
    }
    return CONN_STATE_IDLE;
}

uint8_t BLE_Connection_IsConnected(uint16_t conn_handle)
{
    uint8_t i;
    
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            return (connections[i].state == CONN_STATE_CONNECTED) ? 1U : 0U;
        }
    }
    return 0;
}

void BLE_Connection_OnScanReport(const uint8_t *mac, int8_t rssi, 
                                  const char *name, uint8_t addr_type)
{
    int idx;
    BLE_Device_t *dev;
    
    if (mac == NULL) {
        return;
    }
    
    idx = BLE_DeviceManager_AddDevice(mac, rssi);
    
    if (idx >= 0) {
        dev = BLE_DeviceManager_GetDevice(idx);
        if (dev == NULL) {
            return;
        }
        /*Update Address Type*/
        BLE_DeviceManager_UpdateAddrType(idx, addr_type);
        
        if (name != NULL && name[0] != '\0') {
            /* Update name if available */
            BLE_DeviceManager_UpdateName(idx, name);
        }
        
        /* Send AT response for newly discovered device */
        if (!dev->reported_in_scan) {
            dev->reported_in_scan = 1;
            AT_Response_Send("+SCAN:%02X:%02X:%02X:%02X:%02X:%02X,%d,%s\r\n",
                mac[5], mac[4], mac[3], mac[2], mac[1], mac[0],
                (int)rssi,
                (name != NULL && name[0] != '\0') ? name : "Unknown");
        }
    }
}


void BLE_Connection_OnConnected(const uint8_t *mac, uint16_t conn_handle, uint8_t status)
{
    int dev_idx;
    uint8_t i;
    
    DEBUG_INFO("Conn complete: hdl=0x%04X status=0x%02X", conn_handle, status);
    
    if (status != 0) {
        DEBUG_ERROR("Conn failed: 0x%02X", status);
        AT_Response_Send("+CONN_ERROR:%02X\r\n", status);
        return;
    }
    
    dev_idx = BLE_DeviceManager_FindDevice(mac);
    if (dev_idx >= 0) {
        BLE_DeviceManager_UpdateConnection(dev_idx, conn_handle, 1);
        
        /* Store in connections array */
        for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
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
    int dev_idx;
    uint8_t i;
    
    DEBUG_INFO("Disconn: hdl=0x%04X reason=0x%02X", conn_handle, reason);
    
    dev_idx = BLE_DeviceManager_FindConnHandle(conn_handle);
    if (dev_idx >= 0) {
        BLE_DeviceManager_UpdateConnection(dev_idx, conn_handle, 0);
    }
    
    /* Remove from connections */
    for (i = 0; i < MAX_BLE_CONNECTIONS; i++) {
        if (connections[i].conn_handle == conn_handle) {
            connections[i].conn_handle = 0xFFFF;
            connections[i].state = CONN_STATE_IDLE;
            if (connection_count > 0) {
                connection_count--;
            }
            break;
        }
    }
    
    AT_Response_Send("+DISCONNECTED:0x%04X\r\n", conn_handle);
}
