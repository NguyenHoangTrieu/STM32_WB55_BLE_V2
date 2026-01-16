/**
  ******************************************************************************
  * @file    debug_trace.h
  * @brief   Debug tracing functions (USB CDC only - NO UART!)
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef DEBUG_TRACE_H
#define DEBUG_TRACE_H

#include <stdint.h>
#include <stdio.h>

/**
  * @brief Debug print (uses printf -> USB CDC)
  */
#define DEBUG_PRINT(fmt, ...) do { \
    printf("[DEBUG] "); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\r\n"); \
} while(0)

#define DEBUG_INFO(fmt, ...) do { \
    printf("[INFO] "); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\r\n"); \
} while(0)

#define DEBUG_ERROR(fmt, ...) do { \
    printf("[ERROR] "); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\r\n"); \
} while(0)

#define DEBUG_WARN(fmt, ...) do { \
    printf("[WARN] "); \
    printf(fmt, ##__VA_ARGS__); \
    printf("\r\n"); \
} while(0)

/**
  * @brief Print MAC address
  */
void DEBUG_PrintMAC(const uint8_t *mac);

/**
  * @brief Print hex data
  */
void DEBUG_PrintHEX(const uint8_t *data, uint16_t len);

/**
  * @brief Print connection info
  */
void DEBUG_PrintConnectionInfo(uint16_t conn_handle);

/**
  * @brief Print device list
  */
void DEBUG_PrintDeviceList(void);

#endif /* DEBUG_TRACE_H */
