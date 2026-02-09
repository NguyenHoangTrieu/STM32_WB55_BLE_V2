/**
  ******************************************************************************
  * @file    module_mode.h
  * @brief   Mode Control - Command Mode vs Data Mode (Transparent UART)
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef MODULE_MODE_H
#define MODULE_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Operation modes */
typedef enum {
    MODE_COMMAND = 0,    /* AT command mode - parse AT commands */
    MODE_DATA = 1        /* Data mode - transparent UART to BLE GATT */
} Operation_Mode_t;

/* Escape sequence configuration */
#define ESCAPE_SEQ_CHAR       '+'
#define ESCAPE_SEQ_LENGTH     3
#define ESCAPE_GUARD_TIME_MS  1000  /* 1 second guard time before/after escape */

/**
  * @brief Initialize mode control module
  */
void Module_Mode_Init(void);

/**
  * @brief Enter command mode
  * @return 0 if success, -1 if error
  */
int Module_Mode_EnterCommand(void);

/**
  * @brief Enter data mode (transparent UART<->GATT)
  * @param dev_idx Device index
  * @param char_handle Characteristic handle for data transfer
  * @return 0 if success, -1 if error
  */
int Module_Mode_EnterData(uint8_t dev_idx, uint16_t char_handle);

/**
  * @brief Get current mode
  * @return Current operation mode
  */
Operation_Mode_t Module_Mode_GetCurrent(void);

/**
  * @brief Process incoming UART byte in data mode
  * @param byte Data byte received
  * @note This handles escape sequence detection and data buffering
  */
void Module_Mode_ProcessDataByte(uint8_t byte);

/**
  * @brief Process incoming GATT notification in data mode
  * @param conn_handle Connection handle
  * @param handle Characteristic handle
  * @param data Data bytes
  * @param len Data length
  * @note Forward GATT data to UART in data mode
  */
void Module_Mode_ProcessGATTData(uint16_t conn_handle, uint16_t handle, 
                                 const uint8_t *data, uint16_t len);

/**
  * @brief Check if escape sequence detected
  * @return 1 if escape detected, 0 otherwise
  */
uint8_t Module_Mode_IsEscapeDetected(void);

/**
  * @brief Get data mode target device index
  * @return Device index, or 0xFF if not in data mode
  */
uint8_t Module_Mode_GetTargetDevice(void);

/**
  * @brief Get data mode target characteristic handle
  * @return Characteristic handle, or 0 if not in data mode
  */
uint16_t Module_Mode_GetTargetHandle(void);

/**
  * @brief Flush data mode TX buffer
  * @return Number of bytes flushed
  */
int Module_Mode_FlushTxBuffer(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_MODE_H */
