/**
  ******************************************************************************
  * @file    module_execute.h
  * @brief   BLE Gateway Application Executor - Main initialization and execution
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef MODULE_EXECUTE_H
#define MODULE_EXECUTE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief Initialize all BLE Gateway modules
  * @note  Call this function once during system initialization
  *        This will initialize:
  *        - Device Manager
  *        - AT Command Parser
  *        - BLE Connection Manager
  *        - GATT Client
  *        - Event Handler
  *        - Register sequencer task for AT command processing
  */
void module_ble_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_EXECUTE_H */
