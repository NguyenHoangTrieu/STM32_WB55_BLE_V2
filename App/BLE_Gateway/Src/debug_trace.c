/**
  ******************************************************************************
  * @file    debug_trace.c
  * @brief   Debug tracing implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "debug_trace.h"

void DEBUG_PrintMAC(const uint8_t *mac)
{
    if (!mac) {
        printf("MAC: (NULL)\r\n");
        return;
    }
    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
           mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
}

void DEBUG_PrintHEX(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0) {
        printf("HEX: (empty)\r\n");
        return;
    }
    
    printf("HEX[%d]: ", len);
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");
}

void DEBUG_PrintConnectionInfo(uint16_t conn_handle)
{
    printf("=== Connection Handle: 0x%04X ===\r\n", conn_handle);
}

void DEBUG_PrintDeviceList(void)
{
    printf("=== Device List ===\r\n");
}
