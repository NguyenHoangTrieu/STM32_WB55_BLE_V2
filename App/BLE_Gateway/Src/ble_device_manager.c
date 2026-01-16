/**
  ******************************************************************************
  * @file    ble_device_manager.c
  * @brief   BLE Device Manager implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "ble_device_manager.h"
#include "debug_trace.h"

static BLE_DeviceManager_t device_manager = {0};

void BLE_DeviceManager_Init(void)
{
    memset(&device_manager, 0, sizeof(BLE_DeviceManager_t));
    device_manager.device_count = 0;
    device_manager.scan_active = 0;
    
    // Initialize all connection handles to invalid
    for (int i = 0; i < MAX_BLE_DEVICES; i++) {
        device_manager.devices[i].conn_handle = 0xFFFF;
        device_manager.devices[i].device_index = i;
    }
    
    DEBUG_INFO("Device Manager initialized");
}

int BLE_DeviceManager_AddDevice(const uint8_t *mac, int8_t rssi)
{
    if (!mac) return -1;
    
    // Check if device already exists
    for (int i = 0; i < device_manager.device_count; i++) {
        if (memcmp(device_manager.devices[i].mac_addr, mac, BLE_MAC_LEN) == 0) {
            // Update RSSI
            device_manager.devices[i].rssi = rssi;
            return i;
        }
    }
    
    // Add new device if space available
    if (device_manager.device_count < MAX_BLE_DEVICES) {
        int idx = device_manager.device_count;
        memcpy(device_manager.devices[idx].mac_addr, mac, BLE_MAC_LEN);
        device_manager.devices[idx].rssi = rssi;
        device_manager.devices[idx].is_connected = 0;
        device_manager.devices[idx].conn_handle = 0xFFFF;
        device_manager.device_count++;
        
        DEBUG_INFO("Device added: index=%d, RSSI=%d", idx, rssi);
        return idx;
    }
    
    DEBUG_WARN("Device list full, cannot add device");
    return -1;
}

int BLE_DeviceManager_FindDevice(const uint8_t *mac)
{
    if (!mac) return -1;
    
    for (int i = 0; i < device_manager.device_count; i++) {
        if (memcmp(device_manager.devices[i].mac_addr, mac, BLE_MAC_LEN) == 0) {
            return i;
        }
    }
    
    return -1;
}

int BLE_DeviceManager_FindConnHandle(uint16_t conn_handle)
{
    for (int i = 0; i < device_manager.device_count; i++) {
        if (device_manager.devices[i].conn_handle == conn_handle) {
            return i;
        }
    }
    
    return -1;
}

void BLE_DeviceManager_UpdateConnection(int dev_idx, uint16_t conn_handle, uint8_t connected)
{
    if (dev_idx < 0 || dev_idx >= device_manager.device_count) return;
    
    device_manager.devices[dev_idx].conn_handle = connected ? conn_handle : 0xFFFF;
    device_manager.devices[dev_idx].is_connected = connected;
    
    DEBUG_INFO("Device[%d] connection updated: handle=0x%04X, connected=%d",
               dev_idx, conn_handle, connected);
}

BLE_Device_t* BLE_DeviceManager_GetDevice(int idx)
{
    if (idx < 0 || idx >= device_manager.device_count) return NULL;
    return &device_manager.devices[idx];
}

uint8_t BLE_DeviceManager_GetCount(void)
{
    return device_manager.device_count;
}

void BLE_DeviceManager_PrintList(void)
{
    DEBUG_PRINT("=== BLE Device List (%d devices) ===", device_manager.device_count);
    
    for (int i = 0; i < device_manager.device_count; i++) {
        BLE_Device_t *dev = &device_manager.devices[i];
        printf("[%d] %02X:%02X:%02X:%02X:%02X:%02X | RSSI:%3d | Conn: %s (0x%04X)\r\n",
               i,
               dev->mac_addr[5], dev->mac_addr[4], dev->mac_addr[3],
               dev->mac_addr[2], dev->mac_addr[1], dev->mac_addr[0],
               dev->rssi,
               dev->is_connected ? "YES" : "NO",
               dev->conn_handle);
    }
}

void BLE_DeviceManager_Clear(void)
{
    device_manager.device_count = 0;
    memset(device_manager.devices, 0, sizeof(device_manager.devices));
    DEBUG_INFO("Device list cleared");
}

void BLE_DeviceManager_SetScanActive(uint8_t active)
{
    device_manager.scan_active = active;
}

uint8_t BLE_DeviceManager_IsScanActive(void)
{
    return device_manager.scan_active;
}
