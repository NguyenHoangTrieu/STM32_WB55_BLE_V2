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

/**
  * @brief Initialize GATT client
  */
void BLE_GATT_Init(void);

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

#endif /* BLE_GATT_CLIENT_H */
