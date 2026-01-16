/**
  ******************************************************************************
  * @file    ble_gatt_client.c
  * @brief   GATT Client implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "ble_gatt_client.h"
#include "debug_trace.h"
#include "ble_gatt_aci.h"

typedef struct {
    uint16_t conn_handle;
    BLE_Service_t services[MAX_SERVICES];
    uint8_t service_count;
    BLE_Characteristic_t characteristics[MAX_CHARACTERISTICS];
    uint8_t char_count;
} GATTClientContext_t;

static GATTClientContext_t gatt_context[8] = {0};

void BLE_GATT_Init(void)
{
    for (int i = 0; i < 8; i++) {
        gatt_context[i].conn_handle = 0xFFFF;
        gatt_context[i].service_count = 0;
        gatt_context[i].char_count = 0;
    }
    DEBUG_INFO("GATT Client initialized");
}

int BLE_GATT_DiscoverAllServices(uint16_t conn_handle)
{
    DEBUG_INFO("Discovering services on connection 0x%04X", conn_handle);
    
    // TODO: Implement with actual HCI_LE_READ_REMOTE_USED_FEATURES_COMMAND
    // For now, stub implementation - will be triggered by discovery event
    
    return 0;
}

int BLE_GATT_DiscoverCharacteristics(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle)
{
    (void)conn_handle;
    DEBUG_INFO("Discovering characteristics: 0x%04X-0x%04X", start_handle, end_handle);
    
    // TODO: Implement with HCI Read by Type Request
    // Stub for now
    
    return 0;
}

int BLE_GATT_DiscoverDescriptors(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle)
{
    (void)conn_handle;
    DEBUG_INFO("Discovering descriptors: 0x%04X-0x%04X", start_handle, end_handle);
    
    // TODO: Implement with HCI Find Info Request
    // Stub for now
    
    return 0;
}

int BLE_GATT_ReadCharacteristic(uint16_t conn_handle, uint16_t char_handle)
{
    DEBUG_INFO("Reading characteristic 0x%04X on connection 0x%04X", char_handle, conn_handle);
    
    // TODO: Implement with HCI Read Characteristic Value
    // Stub for now
    
    return 0;
}

int BLE_GATT_WriteCharacteristic(uint16_t conn_handle, uint16_t char_handle,
                                 const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) {
        DEBUG_ERROR("Invalid write data");
        return -1;
    }
    
    DEBUG_INFO("Writing characteristic 0x%04X on connection 0x%04X, len=%d", char_handle, conn_handle, len);
    
    uint8_t ret = aci_gatt_write_char_value(conn_handle, char_handle, len, (uint8_t *)data);
    
    if (ret != 0) {
        DEBUG_ERROR("Failed to write characteristic: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_EnableNotification(uint16_t conn_handle, uint16_t desc_handle)
{
    DEBUG_INFO("Enabling notification: conn=0x%04X, desc=0x%04X", conn_handle, desc_handle);
    
    // Write 0x0001 to CCCD to enable notification
    uint8_t cccd_value[2] = {0x01, 0x00};
    uint8_t ret = aci_gatt_write_char_desc(conn_handle, desc_handle, 2, cccd_value);
    
    if (ret != 0) {
        DEBUG_ERROR("Failed to enable notification: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_DisableNotification(uint16_t conn_handle, uint16_t desc_handle)
{
    DEBUG_INFO("Disabling notification: conn=0x%04X, desc=0x%04X", conn_handle, desc_handle);
    
    // Write 0x0000 to CCCD to disable notification
    uint8_t cccd_value[2] = {0x00, 0x00};
    uint8_t ret = aci_gatt_write_char_desc(conn_handle, desc_handle, 2, cccd_value);
    
    if (ret != 0) {
        DEBUG_ERROR("Failed to disable notification: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

uint8_t BLE_GATT_GetServiceCount(uint16_t conn_handle)
{
    for (int i = 0; i < 8; i++) {
        if (gatt_context[i].conn_handle == conn_handle) {
            return gatt_context[i].service_count;
        }
    }
    return 0;
}

BLE_Service_t* BLE_GATT_GetService(uint16_t conn_handle, uint8_t idx)
{
    for (int i = 0; i < 8; i++) {
        if (gatt_context[i].conn_handle == conn_handle && idx < gatt_context[i].service_count) {
            return &gatt_context[i].services[idx];
        }
    }
    return NULL;
}
