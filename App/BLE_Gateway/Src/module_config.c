/**
  ******************************************************************************
  * @file    module_config.c
  * @brief   Configuration Storage Implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "module_config.h"
#include "debug_trace.h"
#include "main.h"
#include "stm32wbxx_hal.h"
#include "ble_gap_aci.h"
#include "ble_hal_aci.h"
#include <string.h>
#include <stdio.h>

/* Flash configuration - STM32WB55 Flash layout */
/* Use last page of Flash for config storage */
#define FLASH_CONFIG_PAGE_ADDR    0x080FF000  /* Last 4KB page */
#define FLASH_PAGE_SIZE           4096

/* Runtime configuration */
static Module_Config_t current_config;
static uint8_t config_loaded = 0;

/* Default configuration */
static const Module_Config_t default_config = {
    .magic = CONFIG_FLASH_MAGIC,
    .version = 1,
    .device_name = "STM32WB_BLE_GW",
    .uart = {
        .baud_rate = 921600,
        .parity = 0,     /* None */
        .stop_bits = 1,
        .data_bits = 8
    },
    .rf = {
        .tx_power_dbm = 0,           /* 0 dBm */
        .scan_interval = 0x0010,     /* 10ms */
        .scan_window = 0x0010,       /* 10ms */
        .conn_interval_min = 0x0018, /* 30ms */
        .conn_interval_max = 0x0028  /* 50ms */
    },
    .crc = 0  /* Calculated on save */
};

/* External UART handle */
extern UART_HandleTypeDef hlpuart1;

/*============================================================================
 * CRC32 Calculation
 *============================================================================*/
static uint32_t Calculate_CRC32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i, j;
    
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return ~crc;
}

/*============================================================================
 * Initialization
 *============================================================================*/
void Module_Config_Init(void)
{
    // __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
    /* Try to load from Flash */
    // if (Module_Config_Load() != 0) {
        /* Load failed, use defaults */
        DEBUG_WARN("Config load failed, using defaults");
        memcpy(&current_config, &default_config, sizeof(Module_Config_t));
        config_loaded = 1;
    // }
    
    DEBUG_INFO("Config module initialized: %s", current_config.device_name);
}

/*============================================================================
 * Get/Set Functions
 *============================================================================*/
const Module_Config_t* Module_Config_Get(void)
{
    return &current_config;
}

int Module_Config_SetDeviceName(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    
    strncpy(current_config.device_name, name, CONFIG_MAX_DEVICE_NAME_LEN - 1);
    current_config.device_name[CONFIG_MAX_DEVICE_NAME_LEN - 1] = '\0';
    
    DEBUG_INFO("Device name set: %s", current_config.device_name);
    return 0;
}

int Module_Config_GetDeviceName(char *buffer, uint16_t max_len)
{
    if (buffer == NULL || max_len == 0) {
        return -1;
    }
    
    strncpy(buffer, current_config.device_name, max_len - 1);
    buffer[max_len - 1] = '\0';
    
    return strlen(buffer);
}

int Module_Config_SetUART(const UART_Config_t *uart_config)
{
    if (uart_config == NULL) {
        return -1;
    }
    
    /* Validate parameters */
    if (uart_config->baud_rate < 9600 || uart_config->baud_rate > 921600) {
        return -1;
    }
    
    if (uart_config->parity > 2 || uart_config->stop_bits < 1 || 
        uart_config->stop_bits > 2 || uart_config->data_bits < 7 || 
        uart_config->data_bits > 8) {
        return -1;
    }
    
    memcpy(&current_config.uart, uart_config, sizeof(UART_Config_t));
    DEBUG_INFO("UART config updated: %lu baud", uart_config->baud_rate);
    
    return 0;
}

int Module_Config_GetUART(UART_Config_t *uart_config)
{
    if (uart_config == NULL) {
        return -1;
    }
    
    memcpy(uart_config, &current_config.uart, sizeof(UART_Config_t));
    return 0;
}

int Module_Config_SetRF(const RF_Config_t *rf_config)
{
    if (rf_config == NULL) {
        return -1;
    }
    
    /* Validate TX power (-40 to +6 dBm for STM32WB) */
    if (rf_config->tx_power_dbm < -40 || rf_config->tx_power_dbm > 6) {
        return -1;
    }
    
    memcpy(&current_config.rf, rf_config, sizeof(RF_Config_t));
    DEBUG_INFO("RF config updated: TX=%d dBm", rf_config->tx_power_dbm);
    
    return 0;
}

int Module_Config_GetRF(RF_Config_t *rf_config)
{
    if (rf_config == NULL) {
        return -1;
    }
    
    memcpy(rf_config, &current_config.rf, sizeof(RF_Config_t));
    return 0;
}

/*============================================================================
 * NVM Save/Load
 *============================================================================*/
int Module_Config_Save(void)
{
    HAL_StatusTypeDef status;
    uint32_t page_error;
    FLASH_EraseInitTypeDef erase_init;
    uint64_t *src_ptr = (uint64_t*)&current_config;
    uint32_t flash_addr = FLASH_CONFIG_PAGE_ADDR;
    uint32_t len = sizeof(Module_Config_t);
    uint32_t i;
    
    /* Calculate CRC */
    current_config.crc = Calculate_CRC32((uint8_t*)&current_config, 
                                         sizeof(Module_Config_t) - sizeof(uint32_t));
    
    /* Unlock Flash */
    HAL_FLASH_Unlock();
    
    /* Erase page */
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Page = (FLASH_CONFIG_PAGE_ADDR - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase_init.NbPages = 1;
    
    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    if (status != HAL_OK) {
        DEBUG_ERROR("Flash erase failed: %d", status);
        HAL_FLASH_Lock();
        return -1;
    }
    
    /* Write config data (64-bit aligned) */
    for (i = 0; i < (len + 7) / 8; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, 
                                    flash_addr, 
                                    src_ptr[i]);
        if (status != HAL_OK) {
            DEBUG_ERROR("Flash write failed: %d", status);
            HAL_FLASH_Lock();
            return -1;
        }
        flash_addr += 8;
    }
    
    /* Lock Flash */
    HAL_FLASH_Lock();
    
    DEBUG_INFO("Config saved to Flash");
    return 0;
}

int Module_Config_Load(void)
{
    Module_Config_t *flash_config = (Module_Config_t*)FLASH_CONFIG_PAGE_ADDR;
    uint32_t calculated_crc;
    
    /* Check magic number */
    if (flash_config->magic != CONFIG_FLASH_MAGIC) {
        DEBUG_WARN("Invalid config magic: 0x%08lX", flash_config->magic);
        return -1;
    }
    
    /* Verify CRC */
    calculated_crc = Calculate_CRC32((uint8_t*)flash_config, 
                                     sizeof(Module_Config_t) - sizeof(uint32_t));
    if (calculated_crc != flash_config->crc) {
        DEBUG_ERROR("Config CRC mismatch: calc=0x%08lX, stored=0x%08lX", 
                    calculated_crc, flash_config->crc);
        return -1;
    }
    
    /* Copy to RAM */
    memcpy(&current_config, flash_config, sizeof(Module_Config_t));
    config_loaded = 1;
    
    DEBUG_INFO("Config loaded from Flash");
    return 0;
}

void Module_Config_FactoryReset(void)
{
    DEBUG_WARN("Factory reset - restoring defaults");
    
    /* Restore default config */
    memcpy(&current_config, &default_config, sizeof(Module_Config_t));
    
    /* Save to Flash */
    Module_Config_Save();
    
    DEBUG_INFO("Factory reset complete");
}

/*============================================================================
 * Apply Configuration
 *============================================================================*/
int Module_Config_ApplyUART(void)
{
    /* De-init UART */
    HAL_UART_DeInit(&hlpuart1);
    
    /* Update UART parameters */
    hlpuart1.Init.BaudRate = current_config.uart.baud_rate;
    
    /* Parity */
    switch (current_config.uart.parity) {
        case 0: hlpuart1.Init.Parity = UART_PARITY_NONE; break;
        case 1: hlpuart1.Init.Parity = UART_PARITY_EVEN; break;
        case 2: hlpuart1.Init.Parity = UART_PARITY_ODD; break;
        default: hlpuart1.Init.Parity = UART_PARITY_NONE; break;
    }
    
    /* Stop bits */
    hlpuart1.Init.StopBits = (current_config.uart.stop_bits == 2) ? 
                             UART_STOPBITS_2 : UART_STOPBITS_1;
    
    /* Word length */
    if (current_config.uart.parity == 0) {
        hlpuart1.Init.WordLength = (current_config.uart.data_bits == 8) ? 
                                   UART_WORDLENGTH_8B : UART_WORDLENGTH_7B;
    } else {
        /* With parity, need 9-bit word for 8-bit data */
        hlpuart1.Init.WordLength = (current_config.uart.data_bits == 8) ? 
                                   UART_WORDLENGTH_9B : UART_WORDLENGTH_8B;
    }
    
    /* Re-init UART */
    if (HAL_UART_Init(&hlpuart1) != HAL_OK) {
        DEBUG_ERROR("Failed to apply UART config");
        return -1;
    }
    
    DEBUG_INFO("UART config applied: %lu baud", current_config.uart.baud_rate);
    return 0;
}

int Module_Config_ApplyRF(void)
{
    tBleStatus ret;
    
    /* Set TX power */
    ret = aci_hal_set_tx_power_level(1, current_config.rf.tx_power_dbm);
    if (ret != BLE_STATUS_SUCCESS) {
        DEBUG_ERROR("Failed to set TX power: 0x%02X", ret);
        return -1;
    }
    
    DEBUG_INFO("RF config applied: TX=%d dBm", current_config.rf.tx_power_dbm);
    return 0;
}
