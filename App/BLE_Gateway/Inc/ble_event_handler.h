/**
  ******************************************************************************
  * @file    ble_event_handler.h
  * @brief   BLE Event Handler - centralized HCI event callbacks
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef BLE_EVENT_HANDLER_H
#define BLE_EVENT_HANDLER_H

#include <stdint.h>

// Event callback function pointers
typedef void (*BLE_ScanReportCallback_t)(const uint8_t *mac, int8_t rssi, const char *name, uint8_t addr_type);
typedef void (*BLE_ConnectionCompleteCallback_t)(const uint8_t *mac, uint16_t conn_handle, uint8_t status);
typedef void (*BLE_DisconnectionCompleteCallback_t)(uint16_t conn_handle, uint8_t reason);
typedef void (*BLE_GATTCNotificationCallback_t)(uint16_t conn_handle, uint16_t handle,
                                                 const uint8_t *data, uint16_t len);
typedef void (*BLE_GATTCReadResponseCallback_t)(uint16_t conn_handle, uint16_t handle,
                                                 const uint8_t *data, uint16_t len);
typedef void (*BLE_GATTCWriteResponseCallback_t)(uint16_t conn_handle, uint8_t status);

/**
  * @brief Initialize event handler
  */
void BLE_EventHandler_Init(void);

/**
  * @brief Register scan report callback
  */
void BLE_EventHandler_RegisterScanCallback(BLE_ScanReportCallback_t cb);

/**
  * @brief Register connection complete callback
  */
void BLE_EventHandler_RegisterConnectionCallback(BLE_ConnectionCompleteCallback_t cb);

/**
  * @brief Register disconnection callback
  */
void BLE_EventHandler_RegisterDisconnectionCallback(BLE_DisconnectionCompleteCallback_t cb);

/**
  * @brief Register notification callback
  */
void BLE_EventHandler_RegisterNotificationCallback(BLE_GATTCNotificationCallback_t cb);

/**
  * @brief Register read response callback
  */
void BLE_EventHandler_RegisterReadResponseCallback(BLE_GATTCReadResponseCallback_t cb);

/**
  * @brief Register write response callback
  */
void BLE_EventHandler_RegisterWriteResponseCallback(BLE_GATTCWriteResponseCallback_t cb);

/* ============ Event Dispatch Functions ============ */

/**
  * @brief Dispatch scan report event
  */
void BLE_EventHandler_OnScanReport(const uint8_t *mac, int8_t rssi, const char *name, uint8_t addr_type);

/**
  * @brief Dispatch connection complete event
  */
void BLE_EventHandler_OnConnectionComplete(const uint8_t *mac, uint16_t conn_handle, uint8_t status);

/**
  * @brief Dispatch disconnection event
  */
void BLE_EventHandler_OnDisconnectionComplete(uint16_t conn_handle, uint8_t reason);

/**
  * @brief Dispatch notification event
  */
void BLE_EventHandler_OnNotification(uint16_t conn_handle, uint16_t handle,
                                      const uint8_t *data, uint16_t len);

/**
  * @brief Dispatch read response event
  */
void BLE_EventHandler_OnReadResponse(uint16_t conn_handle, uint16_t handle,
                                      const uint8_t *data, uint16_t len);

/**
  * @brief Dispatch write response event
  */
void BLE_EventHandler_OnWriteResponse(uint16_t conn_handle, uint8_t status);

#endif /* BLE_EVENT_HANDLER_H */
