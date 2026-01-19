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

int BLE_GATT_DiscoverAllServices(uint16_t conn_handle)
{
    tBleStatus ret;
    
    DEBUG_INFO("Discovering all services: conn=0x%04X", conn_handle);
    
    /* Start discovering all primary services
     * Response will come via ACI_ATT_READ_BY_GROUP_TYPE_RESP_VSEVT_CODE event
     */
    ret = aci_gatt_disc_all_primary_services(conn_handle);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to start service discovery: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_DiscoverCharacteristics(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle)
{
    tBleStatus ret;
    
    DEBUG_INFO("Discovering chars: conn=0x%04X, start=0x%04X, end=0x%04X", 
               conn_handle, start_handle, end_handle);
    
    /* Discover all characteristics within handle range
     * Response will come via ACI_ATT_READ_BY_TYPE_RESP_VSEVT_CODE event
     */
    ret = aci_gatt_disc_all_char_of_service(conn_handle, start_handle, end_handle);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to start char discovery: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_ReadCharacteristic(uint16_t conn_handle, uint16_t char_handle)
{
    tBleStatus ret;
    
    DEBUG_INFO("Reading char: conn=0x%04X, handle=0x%04X", conn_handle, char_handle);
    
    /* Read characteristic value
     * Response will come via ACI_GATT_NOTIFICATION_VSEVT_CODE or read response event
     */
    ret = aci_gatt_read_char_value(conn_handle, char_handle);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to read characteristic: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_WriteCharacteristic(uint16_t conn_handle, uint16_t char_handle,
                                 const uint8_t *data, uint16_t len)
{
    tBleStatus ret;
    
    if (data == NULL || len == 0) {
        DEBUG_ERROR("Invalid write data");
        return -1;
    }
    
    DEBUG_INFO("Writing char: conn=0x%04X, handle=0x%04X, len=%d", 
               conn_handle, char_handle, len);
    
    /* Write with response (Write Request)
     * Response will come via ACI_GATT_PROC_COMPLETE_VSEVT_CODE event
     */
    ret = aci_gatt_write_char_value(conn_handle, char_handle, len, (uint8_t *)data);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to write characteristic: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_WriteCharacteristicNoResp(uint16_t conn_handle, uint16_t char_handle,
                                       const uint8_t *data, uint16_t len)
{
    tBleStatus ret;
    
    if (data == NULL || len == 0) {
        DEBUG_ERROR("Invalid write data");
        return -1;
    }
    
    DEBUG_INFO("Writing char (no resp): conn=0x%04X, handle=0x%04X, len=%d", 
               conn_handle, char_handle, len);
    
    /* Write without response (Write Command) */
    ret = aci_gatt_write_without_resp(conn_handle, char_handle, len, (uint8_t *)data);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to write characteristic (no resp): 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_EnableNotification(uint16_t conn_handle, uint16_t desc_handle)
{
    tBleStatus ret;
    
    DEBUG_INFO("Enabling notification: conn=0x%04X, desc=0x%04X", conn_handle, desc_handle);
    
    /* Write 0x0001 to CCCD to enable notification
     * Bit 0 = Notification enable
     * Bit 1 = Indication enable
     */
    uint8_t cccd_value[2] = {0x01, 0x00};
    ret = aci_gatt_write_char_desc(conn_handle, desc_handle, 2, cccd_value);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to enable notification: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_DisableNotification(uint16_t conn_handle, uint16_t desc_handle)
{
    tBleStatus ret;
    
    DEBUG_INFO("Disabling notification: conn=0x%04X, desc=0x%04X", conn_handle, desc_handle);
    
    /* Write 0x0000 to CCCD to disable notification */
    uint8_t cccd_value[2] = {0x00, 0x00};
    ret = aci_gatt_write_char_desc(conn_handle, desc_handle, 2, cccd_value);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to disable notification: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}

int BLE_GATT_EnableIndication(uint16_t conn_handle, uint16_t desc_handle)
{
    tBleStatus ret;
    
    DEBUG_INFO("Enabling indication: conn=0x%04X, desc=0x%04X", conn_handle, desc_handle);
    
    /* Write 0x0002 to CCCD to enable indication */
    uint8_t cccd_value[2] = {0x02, 0x00};
    ret = aci_gatt_write_char_desc(conn_handle, desc_handle, 2, cccd_value);
    
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to enable indication: 0x%02X", ret);
        return -1;
    }
    
    return 0;
}
