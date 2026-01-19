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

void BLE_GATT_Init(void)
{
    DEBUG_INFO("GATT Client initialized");
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
