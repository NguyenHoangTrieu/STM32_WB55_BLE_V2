/**
  ******************************************************************************
  * @file    ble_device_manager.c
  * @brief   BLE Device Manager implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "ble_device_manager.h"
#include "debug_trace.h"

static BLE_DeviceManager_t device_manager;
static uint8_t list_full_warned = 0;  /* Flag to avoid spam */

void BLE_DeviceManager_Init(void)
{
    uint8_t i;
    
    memset(&device_manager, 0, sizeof(BLE_DeviceManager_t));
    device_manager.device_count = 0;
    device_manager.scan_active = 0;
    
    /* Initialize all connection handles to invalid */
    for (i = 0; i < MAX_BLE_DEVICES; i++) {
        device_manager.devices[i].conn_handle = 0xFFFF;
        device_manager.devices[i].device_index = i;
        device_manager.devices[i].addr_type = 0x00;
        device_manager.devices[i].name[0] = '\0';
    }
    
    DEBUG_INFO("Device Manager initialized");
}

int BLE_DeviceManager_AddDevice(const uint8_t *mac, int8_t rssi)
{
    uint8_t i;
    uint8_t idx;
    
    if (mac == NULL) {
        return -1;
    }
    
    /* Check if device already exists */
    for (i = 0; i < device_manager.device_count; i++) {
        if (memcmp(device_manager.devices[i].mac_addr, mac, BLE_MAC_LEN) == 0) {
            /* Update RSSI only */
            device_manager.devices[i].rssi = rssi;
            return (int)i;
        }
    }
    
    /* Add new device if space available */
    if (device_manager.device_count < MAX_BLE_DEVICES) {
        idx = device_manager.device_count;
        memcpy(device_manager.devices[idx].mac_addr, mac, BLE_MAC_LEN);
        device_manager.devices[idx].rssi = rssi;
        device_manager.devices[idx].is_connected = 0;
        device_manager.devices[idx].conn_handle = 0xFFFF;
        device_manager.devices[idx].name[0] = '\0';
        device_manager.device_count++;
        list_full_warned = 0;  /* Reset warning flag */
        
        DEBUG_INFO("Device added: idx=%d RSSI=%d", idx, rssi);
        return (int)idx;
    }
    
    /* Log warning only once to avoid spam */
    if (!list_full_warned) {
        DEBUG_WARN("Device list full (%d devices)", MAX_BLE_DEVICES);
        list_full_warned = 1;
    }
    return -1;
}

int BLE_DeviceManager_FindDevice(const uint8_t *mac)
{
    uint8_t i;
    
    if (mac == NULL) {
        return -1;
    }
    
    for (i = 0; i < device_manager.device_count; i++) {
        if (memcmp(device_manager.devices[i].mac_addr, mac, BLE_MAC_LEN) == 0) {
            return (int)i;
        }
    }
    
    return -1;
}

int BLE_DeviceManager_FindConnHandle(uint16_t conn_handle)
{
    uint8_t i;
    
    for (i = 0; i < device_manager.device_count; i++) {
        if (device_manager.devices[i].conn_handle == conn_handle) {
            return (int)i;
        }
    }
    
    return -1;
}

void BLE_DeviceManager_UpdateConnection(int dev_idx, uint16_t conn_handle, uint8_t connected)
{
    if (dev_idx < 0 || dev_idx >= (int)device_manager.device_count) {
        return;
    }
    
    device_manager.devices[dev_idx].conn_handle = connected ? conn_handle : 0xFFFF;
    device_manager.devices[dev_idx].is_connected = connected;
    
    DEBUG_INFO("Dev[%d] conn: handle=0x%04X state=%d", dev_idx, conn_handle, connected);
}

BLE_Device_t* BLE_DeviceManager_GetDevice(int idx)
{
    if (idx < 0 || idx >= (int)device_manager.device_count) {
        return NULL;
    }
    return &device_manager.devices[idx];
}

uint8_t BLE_DeviceManager_GetCount(void)
{
    return device_manager.device_count;
}

void BLE_DeviceManager_PrintList(void)
{
    uint8_t i;
    BLE_Device_t *dev;
    
    DEBUG_PRINT("=== BLE Device List (%d devices) ===", device_manager.device_count);
    
    for (i = 0; i < device_manager.device_count; i++) {
        dev = &device_manager.devices[i];
        DEBUG_PRINT("[%d] %02X:%02X:%02X:%02X:%02X:%02X RSSI:%d Conn:%s",
               i,
               dev->mac_addr[5], dev->mac_addr[4], dev->mac_addr[3],
               dev->mac_addr[2], dev->mac_addr[1], dev->mac_addr[0],
               dev->rssi,
               dev->is_connected ? "Y" : "N");
    }
}

void BLE_DeviceManager_Clear(void)
{
    device_manager.device_count = 0;
    list_full_warned = 0;  /* Reset warning flag */
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

void BLE_DeviceManager_UpdateAddrType(int dev_idx, uint8_t addr_type)
{
    if (dev_idx < 0 || dev_idx >= (int)device_manager.device_count) {
        return;
    }

    device_manager.devices[dev_idx].addr_type = addr_type;
    DEBUG_INFO("Dev[%d] addr_type updated: 0x%02X", dev_idx, addr_type);
}

void BLE_DeviceManager_UpdateName(int dev_idx, const char *name)
{
    if (dev_idx < 0 || dev_idx >= (int)device_manager.device_count) {
        return;
    }
    
    if (name == NULL) {
        device_manager.devices[dev_idx].name[0] = '\0';
        return;
    }
    
    strncpy(device_manager.devices[dev_idx].name, name, BLE_DEVICE_NAME_MAX_LEN - 1);
    device_manager.devices[dev_idx].name[BLE_DEVICE_NAME_MAX_LEN - 1] = '\0';
    DEBUG_INFO("Dev[%d] name updated: %s", dev_idx, name);
}