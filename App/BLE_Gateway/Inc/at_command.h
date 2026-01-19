/**
  ******************************************************************************
  * @file    at_command.h
  * @brief   AT Command Parser - processes LPUART1 AT commands
  * @author  BLE Gateway
  ******************************************************************************
  * 
  * CRITICAL: This module handles UART communication ONLY (NO printf here!)
  * - UART RX: Receive AT commands
  * - UART TX: Send responses (OK/ERROR/+DATA)
  * - NO USB CDC printf() output!
  */

#ifndef AT_COMMAND_H
#define AT_COMMAND_H

#include <stdint.h>

#define AT_CMD_MAX_LEN      128

/**
  * @brief Initialize AT command handler
  */
void AT_Command_Init(void);

/**
  * @brief Process incoming byte from UART
  * @param byte Data byte received
  */
void AT_Command_ReceiveByte(uint8_t byte);

/**
  * @brief Process complete command line
  * @param cmd_line Command string
  */
void AT_Command_Process(const char *cmd_line);

/**
  * @brief Process ready command (called from sequencer task)
  */
void AT_Command_ProcessReady(void);

/**
  * @brief Send response via UART (NO printf!)
  */
void AT_Response_Send(const char *fmt, ...);

/* ============ AT Command Handlers ============ */

/**
  * @brief Start BLE scan
  * @param duration_ms Scan duration in milliseconds
  */
int AT_SCAN_Handler(uint16_t duration_ms);

/**
  * @brief Connect to device
  * @param mac_str MAC address string "AA:BB:CC:DD:EE:FF"
  */
int AT_CONNECT_Handler(const char *mac_str);

/**
  * @brief Disconnect from device
  * @param dev_idx Device index
  */
int AT_DISCONNECT_Handler(uint8_t dev_idx);

/**
  * @brief List connected devices
  */
int AT_LIST_Handler(void);

/**
  * @brief Read characteristic
  * @param dev_idx Device index
  * @param char_handle Characteristic handle
  */
int AT_READ_Handler(uint8_t dev_idx, uint16_t char_handle);

/**
  * @brief Write characteristic
  * @param dev_idx Device index
  * @param char_handle Characteristic handle
  * @param data Hex string data
  */
int AT_WRITE_Handler(uint8_t dev_idx, uint16_t char_handle, const char *data);

/**
  * @brief Enable/disable notification
  * @param dev_idx Device index
  * @param desc_handle Descriptor handle
  * @param enable 1 to enable, 0 to disable
  */
int AT_NOTIFY_Handler(uint8_t dev_idx, uint16_t desc_handle, uint8_t enable);

/**
  * @brief Discover services
  * @param dev_idx Device index
  */
int AT_DISC_Handler(uint8_t dev_idx);

/**
  * @brief Get device info
  * @param dev_idx Device index
  */
int AT_INFO_Handler(uint8_t dev_idx);

#endif /* AT_COMMAND_H */
