/**
  ******************************************************************************
  * @file    module_power.h
  * @brief   Power Management - Sleep Modes and Wake Control
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef MODULE_POWER_H
#define MODULE_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Power modes */
typedef enum {
    POWER_MODE_RUN = 0,          /* Normal run mode */
    POWER_MODE_SLEEP = 1,        /* Sleep mode (WFI) */
    POWER_MODE_STOP0 = 2,        /* Stop0 mode - lowest CPU power */
    POWER_MODE_STOP1 = 3,        /* Stop1 mode - slightly higher power */
    POWER_MODE_STOP2 = 4,        /* Stop2 mode - RAM retention */
    POWER_MODE_STANDBY = 5       /* Standby mode - full shutdown */
} Power_Mode_t;

/* Wake sources */
typedef enum {
    WAKE_SOURCE_NONE = 0x00,
    WAKE_SOURCE_UART = 0x01,     /* Wake on UART RX */
    WAKE_SOURCE_GPIO = 0x02,     /* Wake on GPIO interrupt */
    WAKE_SOURCE_TIMER = 0x04,    /* Wake on RTC timer */
    WAKE_SOURCE_BLE = 0x08       /* Wake on BLE event */
} Wake_Source_t;

/**
  * @brief Initialize power management module
  */
void Module_Power_Init(void);

/**
  * @brief Enter sleep mode
  * @param mode Power mode to enter
  * @param wake_mask Wake source mask (bitwise OR of Wake_Source_t)
  * @param timeout_ms Timeout in milliseconds (0 = no timeout)
  * @return 0 if success, -1 if error
  */
int Module_Power_EnterSleep(Power_Mode_t mode, uint8_t wake_mask, uint32_t timeout_ms);

/**
  * @brief Wake from sleep
  * @note Called automatically on wake interrupt
  * @return Wake source that triggered wake
  */
Wake_Source_t Module_Power_Wake(void);

/**
  * @brief Get current power mode
  * @return Current power mode
  */
Power_Mode_t Module_Power_GetMode(void);

/**
  * @brief Check if system is sleeping
  * @return 1 if sleeping, 0 if awake
  */
uint8_t Module_Power_IsSleeping(void);

/**
  * @brief Configure wake sources
  * @param wake_mask Wake source mask
  * @return 0 if success, -1 if error
  */
int Module_Power_ConfigureWake(uint8_t wake_mask);

/**
  * @brief Disable all peripherals before sleep (except wake sources)
  * @return 0 if success, -1 if error
  */
int Module_Power_DisablePeripherals(void);

/**
  * @brief Re-enable peripherals after wake
  * @return 0 if success, -1 if error
  */
int Module_Power_EnablePeripherals(void);

#ifdef __cplusplus
}
#endif

#endif /* MODULE_POWER_H */
