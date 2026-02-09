/**
  ******************************************************************************
  * @file    module_system.h
  * @brief   System Control - Reset, Version Info, System Status
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef MODULE_SYSTEM_H
#define MODULE_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Version information */
#define MODULE_FW_VERSION_MAJOR    1
#define MODULE_FW_VERSION_MINOR    0
#define MODULE_FW_VERSION_PATCH    0
#define MODULE_BUILD_DATE          __DATE__
#define MODULE_BUILD_TIME          __TIME__

/* System status codes */
typedef enum {
    SYS_STATUS_OK = 0,
    SYS_STATUS_ERROR = -1,
    SYS_STATUS_BUSY = -2,
    SYS_STATUS_NOT_SUPPORTED = -3
} System_Status_t;

/**
  * @brief Initialize system control module
  */
void Module_System_Init(void);

/**
  * @brief Get firmware version string
  * @param buffer Output buffer
  * @param max_len Maximum buffer length
  * @return Number of characters written
  */
int Module_System_GetVersion(char *buffer, uint16_t max_len);

/**
  * @brief Get BLE stack version
  * @param buffer Output buffer
  * @param max_len Maximum buffer length
  * @return Number of characters written
  */
int Module_System_GetBLEVersion(char *buffer, uint16_t max_len);

/**
  * @brief Get module BD address
  * @param addr_type Pointer to store address type
  * @param addr 6-byte buffer for BD address
  * @return 0 if success, -1 if error
  */
int Module_System_GetBDAddr(uint8_t *addr_type, uint8_t addr[6]);

/**
  * @brief Software reset - reset MCU via NVIC
  * @note This function does not return
  */
void Module_System_SoftwareReset(void);

/**
  * @brief Hardware reset via GPIO (if reset pin available)
  * @return 0 if success, -1 if not supported
  */
int Module_System_HardwareReset(void);

/**
  * @brief Factory reset - clear all configuration and bonds
  * @note This will reset device after clearing data
  */
void Module_System_FactoryReset(void);

/**
  * @brief Get system uptime in milliseconds
  * @return Uptime in ms
  */
uint32_t Module_System_GetUptime(void);

/**
  * @brief Get free heap size
  * @return Free heap in bytes
  */
uint32_t Module_System_GetFreeHeap(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_SYSTEM_H */
