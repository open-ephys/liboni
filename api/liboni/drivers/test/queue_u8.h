#ifndef __QUEUE_U8_H__
#define __QUEUE_U8_H__

#include <stdlib.h>
#include <stdint.h>

// Simple uint8_t queue

typedef struct {
    size_t capacity;
    size_t front;
    size_t rear;
    size_t size;
    uint8_t *array;
} queue_u8_t;

queue_u8_t *queue_u8_create(size_t capacity);
void queue_u8_destroy(queue_u8_t *q);
int queue_u8_full(queue_u8_t *q);
int queue_u8_empty(queue_u8_t *q);
int queue_u8_enqueue(queue_u8_t *q, uint8_t item);
int queue_u8_dequeue(queue_u8_t *q, uint8_t *item);
int queue_u8_front(queue_u8_t *q, uint8_t *item);
int queue_u8_rear(queue_u8_t *q, uint8_t *item);

#endif
