#include "queue_u8.h"

queue_u8_t *queue_u8_create(size_t capacity)
{
    queue_u8_t *q = (queue_u8_t *)malloc(sizeof(queue_u8_t));
    q->capacity = capacity;
    q->front = q->size = 0;
    q->rear = capacity - 1;
    q->array = (uint8_t *)malloc(q->capacity * sizeof(int));
    return q;
}

void queue_u8_destroy(queue_u8_t *q)
{
    if (q != NULL) {
        free(q->array);
        free(q);
    }
}

int queue_u8_full(queue_u8_t *q)
{
    return q->size == q->capacity;
}

int queue_u8_empty(queue_u8_t *q)
{
    return q->size == 0;
}

int queue_u8_enqueue(queue_u8_t *q, uint8_t item)
{
    if (queue_u8_full(q))
        return -1;
    q->rear = (q->rear + 1) % q->capacity;
    q->array[q->rear] = item;
    q->size = q->size + 1;
    return 0;
}

int queue_u8_dequeue(queue_u8_t *q, uint8_t *item)
{
    if (queue_u8_empty(q))
        return -1;
    *item = q->array[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size = q->size - 1;

    return 0;
}

int queue_u8_front(queue_u8_t *q, uint8_t *item)
{
    if (queue_u8_empty(q))
        return -1;
    *item = q->array[q->front];
    return 0;
}

int queue_u8_rear(queue_u8_t *q, uint8_t *item)
{
    if (queue_u8_empty(q))
        return -1;
    *item = q->array[q->rear];
    return 0;
}
