/**
  ******************************************************************************
  * @file    module_system.c
  * @brief   System Control Implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "module_system.h"
#include "module_config.h"
#include "debug_trace.h"
#include "main.h"
#include "ble_gap_aci.h"
#include "ble_hal_aci.h"
#include "ble_defs.h"
#include "stm32wbxx_hal.h"
#include <stdio.h>
#include <string.h>

/* System uptime counter (incremented in SysTick or timer) */
static volatile uint32_t system_uptime_ms = 0;

/*============================================================================
 * Initialization
 *============================================================================*/
void Module_System_Init(void)
{
    system_uptime_ms = 0;
    DEBUG_INFO("System module initialized");
}

/*============================================================================
 * Version and Info
 *============================================================================*/
int Module_System_GetVersion(char *buffer, uint16_t max_len)
{
    if (buffer == NULL || max_len == 0) {
        return -1;
    }
    
    return snprintf(buffer, max_len, "v%d.%d.%d-%s-%s",
                    MODULE_FW_VERSION_MAJOR,
                    MODULE_FW_VERSION_MINOR,
                    MODULE_FW_VERSION_PATCH,
                    MODULE_BUILD_DATE,
                    MODULE_BUILD_TIME);
}

int Module_System_GetBLEVersion(char *buffer, uint16_t max_len)
{
    if (buffer == NULL || max_len == 0) {
        return -1;
    }
    
    /* STM32WB BLE Stack version - read from stack info */
    /* For now, return static version */
    return snprintf(buffer, max_len, "STM32WB-BLE-v1.13.0");
}

int Module_System_GetBDAddr(uint8_t *addr_type, uint8_t addr[6])
{
    tBleStatus ret;
    uint8_t data_len;
    
    if (addr_type == NULL || addr == NULL) {
        return -1;
    }
    
    /* Read public BD address from controller config data */
    ret = aci_hal_read_config_data(CONFIG_DATA_PUBLIC_ADDRESS_OFFSET, &data_len, addr);
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to get BD addr: 0x%02X", ret);
        return -1;
    }
    
    /* Address type: 0 = Public */
    *addr_type = 0;
    
    return 0;
}

/*============================================================================
 * Reset Functions
 *============================================================================*/
void Module_System_SoftwareReset(void)
{
    DEBUG_WARN("Software reset requested");
    
    /* Delay to allow UART transmission to complete */
    HAL_Delay(100);
    
    /* Trigger NVIC system reset */
    NVIC_SystemReset();
    
    /* Should never reach here */
    while (1);
}

int Module_System_HardwareReset(void)
{
    /* Hardware reset pin not used in this design */
    /* Return not supported */
    DEBUG_WARN("Hardware reset not supported");
    return -1;
}

void Module_System_FactoryReset(void)
{
    DEBUG_WARN("Factory reset requested");
    
    /* Clear all stored configuration */
    Module_Config_FactoryReset();
    
    /* Clear BLE security database (bonds, keys) */
    aci_gap_clear_security_db();
    
    DEBUG_INFO("Factory reset complete, rebooting...");
    
    /* Delay for UART transmission */
    HAL_Delay(100);
    
    /* Software reset to apply changes */
    NVIC_SystemReset();
    
    /* Should never reach here */
    while (1);
}

/*============================================================================
 * System Status
 *============================================================================*/
uint32_t Module_System_GetUptime(void)
{
    return system_uptime_ms;
}

uint32_t Module_System_GetFreeHeap(void)
{
    /* STM32WB does not have standard heap manager */
    /* Return estimated free memory or 0 if not tracked */
    extern uint8_t _end;   /* End of BSS from linker */
    extern uint8_t _estack; /* End of stack from linker */
    
    uint32_t free_mem = (uint32_t)&_estack - (uint32_t)&_end;
    return free_mem;
}

/*============================================================================
 * Uptime Tick - call from SysTick_Handler
 *============================================================================*/
void Module_System_IncrementUptime(void)
{
    system_uptime_ms++;
}
