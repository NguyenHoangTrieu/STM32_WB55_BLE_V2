/**
  ******************************************************************************
  * @file    module_power.c
  * @brief   Power Management Implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "module_power.h"
#include "debug_trace.h"
#include "main.h"
#include "app_conf.h"
#include "stm32wbxx_hal.h"
#include "stm32_lpm.h"
#include "stm32_lpm_if.h"
#include "ble_connection.h"

/* Power state */
static Power_Mode_t current_mode = POWER_MODE_RUN;
static uint8_t is_sleeping = 0;
static uint8_t wake_sources = 0;
static Wake_Source_t last_wake_source = WAKE_SOURCE_NONE;

/* External handles */
extern UART_HandleTypeDef hlpuart1;
extern RTC_HandleTypeDef hrtc;

/*============================================================================
 * Initialization
 *============================================================================*/
void Module_Power_Init(void)
{
    current_mode = POWER_MODE_RUN;
    is_sleeping = 0;
    wake_sources = WAKE_SOURCE_UART | WAKE_SOURCE_BLE;  /* Default wake sources */
    
    /* Initialize Low Power Manager */
    UTIL_LPM_Init();
    
    DEBUG_INFO("Power module initialized");
}

/*============================================================================
 * Power Mode Control
 *============================================================================*/
int Module_Power_EnterSleep(Power_Mode_t mode, uint8_t wake_mask, uint32_t timeout_ms)
{
    /* Suppress unused parameter warning - timeout handled by RTC if configured */
    (void)timeout_ms;
    
    if (mode == POWER_MODE_RUN) {
        return 0;  /* Already in run mode */
    }
    
    DEBUG_INFO("Entering sleep mode %d", mode);
    
    /* Configure wake sources */
    wake_sources = wake_mask;
    Module_Power_ConfigureWake(wake_mask);
    
    /* Disable non-essential peripherals */
    Module_Power_DisablePeripherals();
    
    /* Set sleep flag */
    is_sleeping = 1;
    current_mode = mode;
    
    /* Enter low power mode based on type */
    switch (mode) {
        case POWER_MODE_SLEEP:
            /* Sleep mode - WFI */
            UTIL_LPM_SetOffMode((1 << CFG_LPM_APP), UTIL_LPM_DISABLE);
            UTIL_LPM_SetStopMode((1 << CFG_LPM_APP), UTIL_LPM_DISABLE);
            UTIL_LPM_EnterLowPower();
            break;
            
        case POWER_MODE_STOP0:
        case POWER_MODE_STOP1:
        case POWER_MODE_STOP2:
            /* Stop modes with RAM retention */
            UTIL_LPM_SetOffMode((1 << CFG_LPM_APP), UTIL_LPM_DISABLE);
            UTIL_LPM_SetStopMode((1 << CFG_LPM_APP), UTIL_LPM_ENABLE);
            UTIL_LPM_EnterLowPower();
            break;
            
        case POWER_MODE_STANDBY:
            /* Standby - full shutdown */
            DEBUG_WARN("Standby mode not fully supported");
            HAL_PWR_EnterSTANDBYMode();
            break;
            
        default:
            DEBUG_ERROR("Invalid power mode: %d", mode);
            is_sleeping = 0;
            current_mode = POWER_MODE_RUN;
            return -1;
    }
    
    /* After wake - re-enable peripherals */
    is_sleeping = 0;
    current_mode = POWER_MODE_RUN;
    Module_Power_EnablePeripherals();
    
    DEBUG_INFO("Woke from sleep");
    return 0;
}

Wake_Source_t Module_Power_Wake(void)
{
    /* Determine wake source */
    /* This is called from interrupt context */
    
    /* Check UART wake */
    if (__HAL_UART_GET_FLAG(&hlpuart1, UART_FLAG_RXNE)) {
        last_wake_source = WAKE_SOURCE_UART;
    }
    /* Check if BLE event pending */
    else if (/* BLE event check */ 0) {
        last_wake_source = WAKE_SOURCE_BLE;
    }
    /* Check RTC timer */
    else if (__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF)) {
        last_wake_source = WAKE_SOURCE_TIMER;
    }
    else {
        last_wake_source = WAKE_SOURCE_GPIO;
    }
    
    return last_wake_source;
}

Power_Mode_t Module_Power_GetMode(void)
{
    return current_mode;
}

uint8_t Module_Power_IsSleeping(void)
{
    return is_sleeping;
}

/*============================================================================
 * Wake Configuration
 *============================================================================*/
int Module_Power_ConfigureWake(uint8_t wake_mask)
{
    wake_sources = wake_mask;
    
    /* Configure UART wake */
    if (wake_mask & WAKE_SOURCE_UART) {
        /* Enable UART RX interrupt */
        __HAL_UART_ENABLE_IT(&hlpuart1, UART_IT_RXNE);
    }
    
    /* Configure GPIO wake */
    if (wake_mask & WAKE_SOURCE_GPIO) {
        /* Enable EXTI interrupts on wake pins */
        /* Implementation depends on specific GPIO configuration */
    }
    
    /* Configure RTC timer wake */
    if (wake_mask & WAKE_SOURCE_TIMER) {
        /* Configure RTC alarm */
        /* Implementation depends on timeout requirement */
    }
    
    /* BLE wake is always enabled via STM32_WPAN */
    
    DEBUG_INFO("Wake sources configured: 0x%02X", wake_mask);
    return 0;
}

/*============================================================================
 * Peripheral Control
 *============================================================================*/
int Module_Power_DisablePeripherals(void)
{
    /* Disable USB CDC (debug) */
    /* Keep LPUART1 enabled if UART wake is configured */
    
    if (!(wake_sources & WAKE_SOURCE_UART)) {
        /* Disable UART if not needed for wake */
        HAL_UART_MspDeInit(&hlpuart1);
    }
    
    /* Stop any active BLE scanning/advertising if going to deep sleep */
    if (current_mode >= POWER_MODE_STOP2) {
        BLE_Connection_StopScan();
    }
    
    DEBUG_INFO("Peripherals disabled for sleep");
    return 0;
}

int Module_Power_EnablePeripherals(void)
{
    /* Re-enable UART if it was disabled */
    if (!(wake_sources & WAKE_SOURCE_UART)) {
        HAL_UART_MspInit(&hlpuart1);
    }
    
    /* Re-enable USB CDC */
    /* USB will auto-reconnect on bus activity */
    
    DEBUG_INFO("Peripherals enabled after wake");
    return 0;
}
