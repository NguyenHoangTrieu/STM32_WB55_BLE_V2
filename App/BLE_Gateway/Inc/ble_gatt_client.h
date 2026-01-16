/**
  ******************************************************************************
  * @file    ble_gatt_client.h
  * @brief   GATT Client operations - read/write/discover characteristics
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef BLE_GATT_CLIENT_H
#define BLE_GATT_CLIENT_H

#include <stdint.h>

#define MAX_SERVICES        16
#define MAX_CHARACTERISTICS 32

typedef struct {
    uint16_t service_handle;
    uint16_t start_handle;
    uint16_t end_handle;
    uint8_t  uuid_type;         // 1 = 16-bit, 16 = 128-bit
    uint8_t  uuid[16];
} BLE_Service_t;

typedef struct {
    uint16_t char_handle;
    uint16_t value_handle;
    uint8_t  properties;
    uint8_t  uuid_type;
    uint8_t  uuid[16];
} BLE_Characteristic_t;

/**
  * @brief Initialize GATT client
  */
void BLE_GATT_Init(void);

/**
  * @brief Start discover all services
  * @param conn_handle Connection handle
  * @return 0 if success
  */
int BLE_GATT_DiscoverAllServices(uint16_t conn_handle);

/**
  * @brief Discover characteristics of service
  * @param conn_handle Connection handle
  * @param start_handle Service start handle
  * @param end_handle Service end handle
  * @return 0 if success
  */
int BLE_GATT_DiscoverCharacteristics(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle);

/**
  * @brief Discover descriptors
  * @param conn_handle Connection handle
  * @param start_handle Characteristic start handle
  * @param end_handle Characteristic end handle
  * @return 0 if success
  */
int BLE_GATT_DiscoverDescriptors(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle);

/**
  * @brief Read characteristic value
  * @param conn_handle Connection handle
  * @param char_handle Characteristic handle
  * @return 0 if success
  */
int BLE_GATT_ReadCharacteristic(uint16_t conn_handle, uint16_t char_handle);

/**
  * @brief Write characteristic value
  * @param conn_handle Connection handle
  * @param char_handle Characteristic handle
  * @param data Data to write
  * @param len Data length
  * @return 0 if success
  */
int BLE_GATT_WriteCharacteristic(uint16_t conn_handle, uint16_t char_handle,
                                 const uint8_t *data, uint16_t len);

/**
  * @brief Enable notification on characteristic
  * @param conn_handle Connection handle
  * @param desc_handle CCCD descriptor handle
  * @return 0 if success
  */
int BLE_GATT_EnableNotification(uint16_t conn_handle, uint16_t desc_handle);

/**
  * @brief Disable notification on characteristic
  * @param conn_handle Connection handle
  * @param desc_handle CCCD descriptor handle
  * @return 0 if success
  */
int BLE_GATT_DisableNotification(uint16_t conn_handle, uint16_t desc_handle);

/**
  * @brief Get discovered services
  * @param conn_handle Connection handle
  * @return Number of services discovered
  */
uint8_t BLE_GATT_GetServiceCount(uint16_t conn_handle);

/**
  * @brief Get service by index
  */
BLE_Service_t* BLE_GATT_GetService(uint16_t conn_handle, uint8_t idx);

#endif /* BLE_GATT_CLIENT_H */
