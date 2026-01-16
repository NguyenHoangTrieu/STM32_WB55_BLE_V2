/**
  ******************************************************************************
  * @file    ble_connection.h
  * @brief   BLE Connection Management - handles multi-device connections
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef BLE_CONNECTION_H
#define BLE_CONNECTION_H

#include <stdint.h>
#include <stdio.h>

#define BLE_MAC_LEN         6
#define MAX_BLE_CONNECTIONS 8

typedef enum {
    CONN_STATE_IDLE,
    CONN_STATE_CONNECTING,
    CONN_STATE_CONNECTED,
    CONN_STATE_DISCOVERING,
    CONN_STATE_DISCONNECTING,
} BLE_ConnectionState_t;

/**
  * @brief Initialize connection manager
  */
void BLE_Connection_Init(void);

/**
  * @brief Start BLE scan
  * @param duration_ms Scan duration in ms
  */
int BLE_Connection_StartScan(uint16_t duration_ms);

/**
  * @brief Stop BLE scan
  */
int BLE_Connection_StopScan(void);

/**
  * @brief Create connection to device
  * @param mac MAC address (6 bytes)
  * @return 0 if success, -1 if error
  */
int BLE_Connection_CreateConnection(const uint8_t *mac);

/**
  * @brief Terminate connection
  * @param conn_handle Connection handle
  * @return 0 if success, -1 if error
  */
int BLE_Connection_TerminateConnection(uint16_t conn_handle);

/**
  * @brief Set connection state
  */
void BLE_Connection_SetState(uint16_t conn_handle, BLE_ConnectionState_t state);

/**
  * @brief Get connection state
  */
BLE_ConnectionState_t BLE_Connection_GetState(uint16_t conn_handle);

/**
  * @brief Check if connected
  */
uint8_t BLE_Connection_IsConnected(uint16_t conn_handle);

/**
  * @brief Callback when scan discovers device
  * @param mac MAC address
  * @param rssi RSSI value
  */
void BLE_Connection_OnScanReport(const uint8_t *mac, int8_t rssi);

/**
  * @brief Callback when connection established
  * @param mac MAC address
  * @param conn_handle Connection handle
  * @param status HCI status
  */
void BLE_Connection_OnConnected(const uint8_t *mac, uint16_t conn_handle, uint8_t status);

/**
  * @brief Callback when disconnected
  * @param conn_handle Connection handle
  * @param reason HCI reason code
  */
void BLE_Connection_OnDisconnected(uint16_t conn_handle, uint8_t reason);

#endif /* BLE_CONNECTION_H */
