/**
  ******************************************************************************
  * @file    circular_buffer.h
  * @brief   Circular buffer for UART RX buffering
  * @author  BLE Gateway
  ******************************************************************************
  */

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t *buffer;
    uint16_t size;
    uint16_t head;   // Write pointer
    uint16_t tail;   // Read pointer
} CircularBuffer_t;

/**
  * @brief Initialize circular buffer
  */
void CircularBuffer_Init(CircularBuffer_t *cb, uint8_t *buf, uint16_t size);

/**
  * @brief Put single byte into buffer
  */
void CircularBuffer_Put(CircularBuffer_t *cb, uint8_t data);

/**
  * @brief Get single byte from buffer
  * @return 0 if success, -1 if buffer empty
  */
int CircularBuffer_Get(CircularBuffer_t *cb, uint8_t *data);

/**
  * @brief Get full line until \n or \r\n
  * @param line Output buffer
  * @param max_len Maximum length to read
  * @return Number of bytes read (0 if no complete line), -1 if buffer overflow
  */
int CircularBuffer_GetLine(CircularBuffer_t *cb, char *line, uint16_t max_len);

/**
  * @brief Check if buffer has data
  * @return 1 if has data, 0 if empty
  */
int CircularBuffer_HasData(CircularBuffer_t *cb);

/**
  * @brief Get number of bytes available
  */
uint16_t CircularBuffer_Available(CircularBuffer_t *cb);

/**
  * @brief Clear buffer
  */
void CircularBuffer_Clear(CircularBuffer_t *cb);

#endif /* CIRCULAR_BUFFER_H */
