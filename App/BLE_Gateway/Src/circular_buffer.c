/**
  ******************************************************************************
  * @file    circular_buffer.c
  * @brief   Circular buffer implementation
  * @author  BLE Gateway
  ******************************************************************************
  */

#include "circular_buffer.h"

void CircularBuffer_Init(CircularBuffer_t *cb, uint8_t *buf, uint16_t size)
{
    if (cb && buf && size > 0) {
        cb->buffer = buf;
        cb->size = size;
        cb->head = 0;
        cb->tail = 0;
    }
}

void CircularBuffer_Put(CircularBuffer_t *cb, uint8_t data)
{
    if (!cb || !cb->buffer) return;
    
    cb->buffer[cb->head] = data;
    cb->head = (cb->head + 1) % cb->size;
    
    // Overwrite tail if buffer full (circular behavior)
    if (cb->head == cb->tail) {
        cb->tail = (cb->tail + 1) % cb->size;
    }
}

int CircularBuffer_Get(CircularBuffer_t *cb, uint8_t *data)
{
    if (!cb || !cb->buffer || !data) return -1;
    
    // Check if empty
    if (cb->head == cb->tail) {
        return -1;
    }
    
    *data = cb->buffer[cb->tail];
    cb->tail = (cb->tail + 1) % cb->size;
    return 0;
}

int CircularBuffer_GetLine(CircularBuffer_t *cb, char *line, uint16_t max_len)
{
    if (!cb || !cb->buffer || !line || max_len == 0) return -1;
    
    uint16_t pos = 0;
    uint16_t idx = cb->tail;
    
    // Search for \n or \r\n
    while (idx != cb->head && pos < max_len - 1) {
        uint8_t ch = cb->buffer[idx];
        
        // Found newline
        if (ch == '\n' || ch == '\r') {
            // Add to line
            line[pos++] = ch;
            idx = (idx + 1) % cb->size;
            
            // Skip \n if previous was \r
            if (ch == '\r' && idx != cb->head && cb->buffer[idx] == '\n') {
                line[pos++] = '\n';
                idx = (idx + 1) % cb->size;
            }
            
            line[pos] = '\0';
            cb->tail = idx;
            return pos;
        }
        
        line[pos++] = ch;
        idx = (idx + 1) % cb->size;
    }
    
    // No complete line found
    return 0;
}

int CircularBuffer_HasData(CircularBuffer_t *cb)
{
    if (!cb || !cb->buffer) return 0;
    return cb->head != cb->tail ? 1 : 0;
}

uint16_t CircularBuffer_Available(CircularBuffer_t *cb)
{
    if (!cb || !cb->buffer) return 0;
    
    if (cb->head >= cb->tail) {
        return cb->head - cb->tail;
    } else {
        return cb->size - cb->tail + cb->head;
    }
}

void CircularBuffer_Clear(CircularBuffer_t *cb)
{
    if (!cb || !cb->buffer) return;
    cb->head = 0;
    cb->tail = 0;
}
