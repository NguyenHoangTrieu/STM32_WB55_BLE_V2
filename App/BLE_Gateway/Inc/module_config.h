/**
  ******************************************************************************
  * @file    module_config.h
  * @brief   Configuration Storage - NVM, Device Name, UART/RF Parameters
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef MODULE_CONFIG_H
#define MODULE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Configuration limits */
#define CONFIG_MAX_DEVICE_NAME_LEN    32
#define CONFIG_MAX_BAUD_RATES         8
#define CONFIG_FLASH_MAGIC            0xBE11CAFE  /* Magic number for valid config */

/* UART configuration */
typedef struct {
    uint32_t baud_rate;     /* 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600 */
    uint8_t parity;         /* 0=None, 1=Even, 2=Odd */
    uint8_t stop_bits;      /* 1 or 2 */
    uint8_t data_bits;      /* 7 or 8 */
} UART_Config_t;

/* RF parameters */
typedef struct {
    int8_t tx_power_dbm;    /* -40 to +6 dBm */
    uint16_t scan_interval; /* Scan interval in 0.625ms units (default 0x0010 = 10ms) */
    uint16_t scan_window;   /* Scan window in 0.625ms units (default 0x0010 = 10ms) */
    uint16_t conn_interval_min; /* Connection interval min (default 0x0018 = 30ms) */
    uint16_t conn_interval_max; /* Connection interval max (default 0x0028 = 50ms) */
} RF_Config_t;

/* Persistent configuration structure */
typedef struct {
    uint32_t magic;         /* Magic number to validate config */
    uint32_t version;       /* Config version */
    char device_name[CONFIG_MAX_DEVICE_NAME_LEN];
    UART_Config_t uart;
    RF_Config_t rf;
    uint32_t crc;           /* CRC32 of config data */
} Module_Config_t;

/**
  * @brief Initialize configuration module
  * @note Loads config from NVM or uses defaults
  */
void Module_Config_Init(void);

/**
  * @brief Get current configuration
  * @return Pointer to config structure (read-only)
  */
const Module_Config_t* Module_Config_Get(void);

/**
  * @brief Set device name
  * @param name Device name string (max 31 chars + null)
  * @return 0 if success, -1 if error
  */
int Module_Config_SetDeviceName(const char *name);

/**
  * @brief Get device name
  * @param buffer Output buffer
  * @param max_len Maximum buffer length
  * @return Number of characters written
  */
int Module_Config_GetDeviceName(char *buffer, uint16_t max_len);

/**
  * @brief Set UART configuration
  * @param uart_config UART configuration structure
  * @return 0 if success, -1 if error
  */
int Module_Config_SetUART(const UART_Config_t *uart_config);

/**
  * @brief Get UART configuration
  * @param uart_config Output buffer for UART config
  * @return 0 if success, -1 if error
  */
int Module_Config_GetUART(UART_Config_t *uart_config);

/**
  * @brief Set RF parameters
  * @param rf_config RF configuration structure
  * @return 0 if success, -1 if error
  */
int Module_Config_SetRF(const RF_Config_t *rf_config);

/**
  * @brief Get RF parameters
  * @param rf_config Output buffer for RF config
  * @return 0 if success, -1 if error
  */
int Module_Config_GetRF(RF_Config_t *rf_config);

/**
  * @brief Save current configuration to NVM
  * @return 0 if success, -1 if error
  */
int Module_Config_Save(void);

/**
  * @brief Load configuration from NVM
  * @return 0 if success, -1 if error or invalid config
  */
int Module_Config_Load(void);

/**
  * @brief Factory reset - restore default configuration
  */
void Module_Config_FactoryReset(void);

/**
  * @brief Apply current UART configuration to hardware
  * @return 0 if success, -1 if error
  */
int Module_Config_ApplyUART(void);

/**
  * @brief Apply current RF configuration to BLE stack
  * @return 0 if success, -1 if error
  */
int Module_Config_ApplyRF(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_CONFIG_H */
