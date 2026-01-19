/**
  ******************************************************************************
  * @file    ble_device_manager.h
  * @brief   BLE Device Manager - manages discovered and connected devices
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef BLE_DEVICE_MANAGER_H
#define BLE_DEVICE_MANAGER_H

#include <stdint.h>
#include <string.h>

#define MAX_BLE_DEVICES     32  /* Maximum devices in scan list */
#define BLE_MAC_LEN         6

typedef struct {
    uint8_t   mac_addr[BLE_MAC_LEN];    // MAC address
    uint16_t  conn_handle;              // Connection handle (0xFFFF = not connected)
    uint8_t   is_connected;             // Connection status
    int8_t    rssi;                     // Last RSSI value
    uint8_t   device_index;             // Index in list
} BLE_Device_t;

typedef struct {
    BLE_Device_t devices[MAX_BLE_DEVICES];
    uint8_t      device_count;          // Number of devices in list
    uint8_t      scan_active;           // Scan running?
} BLE_DeviceManager_t;

/**
  * @brief Initialize device manager
  */
void BLE_DeviceManager_Init(void);

/**
  * @brief Add or update device from scan
  * @param mac MAC address (6 bytes)
  * @param rssi RSSI value
  * @return Device index, or -1 if list full
  */
int BLE_DeviceManager_AddDevice(const uint8_t *mac, int8_t rssi);

/**
  * @brief Find device by MAC address
  * @return Device index (0-7), or -1 if not found
  */
int BLE_DeviceManager_FindDevice(const uint8_t *mac);

/**
  * @brief Find device by connection handle
  * @return Device index, or -1 if not found
  */
int BLE_DeviceManager_FindConnHandle(uint16_t conn_handle);

/**
  * @brief Update device connection status
  */
void BLE_DeviceManager_UpdateConnection(int dev_idx, uint16_t conn_handle, uint8_t connected);

/**
  * @brief Get device by index
  */
BLE_Device_t* BLE_DeviceManager_GetDevice(int idx);

/**
  * @brief Get total device count
  */
uint8_t BLE_DeviceManager_GetCount(void);

/**
  * @brief Print all devices to USB CDC
  */
void BLE_DeviceManager_PrintList(void);

/**
  * @brief Clear all devices
  */
void BLE_DeviceManager_Clear(void);

/**
  * @brief Set scan active state
  */
void BLE_DeviceManager_SetScanActive(uint8_t active);

/**
  * @brief Get scan active state
  */
uint8_t BLE_DeviceManager_IsScanActive(void);

#endif /* BLE_DEVICE_MANAGER_H */
