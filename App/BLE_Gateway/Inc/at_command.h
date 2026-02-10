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
  * @brief Discover characteristics
  * @param dev_idx Device index
  * @param start_handle Start handle
  * @param end_handle End handle
  */
int AT_CHARS_Handler(uint8_t dev_idx, uint16_t start_handle, uint16_t end_handle);

/**
  * @brief Get device info
  * @param dev_idx Device index
  */
int AT_INFO_Handler(uint8_t dev_idx);

/* ============ System/Lifecycle Commands ============ */

/**
  * @brief Software reset
  */
int AT_RESET_Handler(void);

/**
  * @brief Hardware reset
  */
int AT_HWRESET_Handler(void);

/**
  * @brief Factory reset
  */
int AT_FACTORY_Handler(void);

/* ============ Info/Config Commands ============ */

/**
  * @brief Get module information (version, BLE stack, BD addr)
  */
int AT_GETINFO_Handler(void);

/**
  * @brief Set device name
  * @param name Device name string
  */
int AT_NAME_Handler(const char *name);

/**
  * @brief Set UART communication parameters
  * @param baud Baud rate
  * @param parity Parity (0=None, 1=Even, 2=Odd)
  * @param stop Stop bits (1 or 2)
  */
int AT_COMM_Handler(uint32_t baud, uint8_t parity, uint8_t stop);

/**
  * @brief Set RF parameters
  * @param tx_power TX power in dBm
  * @param scan_interval Scan interval (0.625ms units)
  * @param scan_window Scan window (0.625ms units)
  */
int AT_RF_Handler(int8_t tx_power, uint16_t scan_interval, uint16_t scan_window);

/**
  * @brief Save configuration to NVM
  */
int AT_SAVE_Handler(void);

/* ============ Mode Commands ============ */

/**
  * @brief Enter command mode
  */
int AT_CMDMODE_Handler(void);

/**
  * @brief Enter data mode (transparent UART<->GATT)
  * @param dev_idx Device index
  * @param char_handle Characteristic handle
  */
int AT_DATAMODE_Handler(uint8_t dev_idx, uint16_t char_handle);

/* ============ Connection Status Commands ============ */

/**
  * @brief Get connection status
  * @param dev_idx Device index (0xFF for all)
  */
int AT_STATUS_Handler(uint8_t dev_idx);

/* ============ Power Management Commands ============ */

/**
  * @brief Enter sleep mode
  * @param mode Sleep mode (1=Sleep, 2=Stop0, 3=Stop1, 4=Stop2)
  * @param wake_mask Wake source mask
  * @param timeout_ms Timeout in milliseconds
  */
int AT_SLEEP_Handler(uint8_t mode, uint8_t wake_mask, uint32_t timeout_ms);

/**
  * @brief Wake from sleep (manual wake command)
  */
int AT_WAKE_Handler(void);

/* ============ Diagnostics Commands ============ */

/**
  * @brief Get diagnostics (RSSI, TX power, connection params)
  * @param dev_idx Device index
  */
int AT_DIAG_Handler(uint8_t dev_idx);

#endif /* AT_COMMAND_H */
