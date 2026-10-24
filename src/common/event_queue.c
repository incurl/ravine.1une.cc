/*
 * event_queue.c — cross-platform event queue implementation.
 *
 * The queue is opaque to its users: event_queue.h declares an
 * incomplete struct and this file completes it privately.
 *
 * Two backends live behind the same API:
 *
 *   BOARD_ESP32S3    The queue is a FreeRTOS queue (xQueueCreate and
 *                    friends). That gives real cross-task safety for
 *                    free: the keypad scan task pushes and the UI task
 *                    pops without a lock.
 *
 *   BOARD_SIMULATOR  The queue is a plain ring buffer over a fixed
 *                    block of memory. The simulator is a single-threaded
 *                    event loop, so a lock would be dead weight.
 *
 * Both implementations:
 *   - Never allocate after creation.
 *   - Refuse to overwrite an unconsumed item: a push to a full queue
 *     returns SYS_ERR_BUSY and drops the new item.
 */
#include "boards/synth_board_select.h"

#include "common/event_queue.h"

#include <stdlib.h>
#include <string.h>

#if defined(BOARD_ESP32S3)

/* ─── ESP32-S3: FreeRTOS queue ─────────────────────────────────── */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

struct event_queue {
    QueueHandle_t handle;
    size_t        item_size;
    size_t        capacity;
};

sys_status_t event_queue_create(size_t item_size,
                                size_t capacity,
                                event_queue_t **out_queue)
{
    if (out_queue == NULL || item_size == 0 || capacity == 0) {
        return SYS_ERR_INVALID_ARG;
    }
    *out_queue = NULL;

    event_queue_t *q = calloc(1, sizeof(*q));
    if (q == NULL) {
        return SYS_ERR_NO_MEM;
    }

    q->handle = xQueueCreate((UBaseType_t)capacity, (UBaseType_t)item_size);
    if (q->handle == NULL) {
        free(q);
        return SYS_ERR_NO_MEM;
    }

    q->item_size = item_size;
    q->capacity  = capacity;
    *out_queue = q;
    return SYS_OK;
}

void event_queue_destroy(event_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }
    if (queue->handle != NULL) {
        vQueueDelete(queue->handle);
    }
    free(queue);
}

sys_status_t event_queue_push(event_queue_t *queue, const void *item)
{
    if (queue == NULL || item == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    /* Zero timeout: never block the producer. A full queue is a real
     * condition the caller should see, not something to hide. */
    if (xQueueSend(queue->handle, item, 0) != pdTRUE) {
        return SYS_ERR_BUSY;
    }
    return SYS_OK;
}

sys_status_t event_queue_pop(event_queue_t *queue, void *out)
{
    if (queue == NULL || out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (xQueueReceive(queue->handle, out, 0) != pdTRUE) {
        return SYS_ERR_NOT_FOUND;
    }
    return SYS_OK;
}

sys_status_t event_queue_peek(const event_queue_t *queue, void *out)
{
    if (queue == NULL || out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (xQueuePeek(queue->handle, out, 0) != pdTRUE) {
        return SYS_ERR_NOT_FOUND;
    }
    return SYS_OK;
}

size_t event_queue_count(const event_queue_t *queue)
{
    if (queue == NULL) {
        return 0;
    }
    return (size_t)uxQueueMessagesWaiting(queue->handle);
}

size_t event_queue_capacity(const event_queue_t *queue)
{
    return (queue == NULL) ? 0 : queue->capacity;
}

bool event_queue_is_empty(const event_queue_t *queue)
{
    return event_queue_count(queue) == 0;
}

void event_queue_clear(event_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }
    xQueueReset(queue->handle);
}
#else /* BOARD_SIMULATOR */

/* ─── Simulator: single-threaded ring buffer ───────────────────── */

struct event_queue {
    uint8_t *storage;
    size_t   item_size;
    size_t   capacity;
    size_t   head;      /* index of the next item to pop      */
    size_t   count;     /* number of items currently queued  */
};

sys_status_t event_queue_create(size_t item_size,
                                size_t capacity,
                                event_queue_t **out_queue)
{
    if (out_queue == NULL || item_size == 0 || capacity == 0) {
        return SYS_ERR_INVALID_ARG;
    }
    *out_queue = NULL;

    event_queue_t *q = calloc(1, sizeof(*q));
    if (q == NULL) {
        return SYS_ERR_NO_MEM;
    }

    q->storage = calloc(capacity, item_size);
    if (q->storage == NULL) {
        free(q);
        return SYS_ERR_NO_MEM;
    }

    q->item_size = item_size;
    q->capacity  = capacity;
    q->head      = 0;
    q->count     = 0;
    *out_queue = q;
    return SYS_OK;
}

void event_queue_destroy(event_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }
    free(queue->storage);
    free(queue);
}

sys_status_t event_queue_push(event_queue_t *queue, const void *item)
{
    if (queue == NULL || item == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (queue->count >= queue->capacity) {
        /* Full: drop the new item rather than an unconsumed one. */
        return SYS_ERR_BUSY;
    }

    size_t tail = (queue->head + queue->count) % queue->capacity;
    memcpy(queue->storage + (tail * queue->item_size), item, queue->item_size);
    queue->count++;
    return SYS_OK;
}

sys_status_t event_queue_pop(event_queue_t *queue, void *out)
{
    if (queue == NULL || out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (queue->count == 0) {
        return SYS_ERR_NOT_FOUND;
    }

    memcpy(out, queue->storage + (queue->head * queue->item_size),
           queue->item_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return SYS_OK;
}

sys_status_t event_queue_peek(const event_queue_t *queue, void *out)
{
    if (queue == NULL || out == NULL) {
        return SYS_ERR_INVALID_ARG;
    }
    if (queue->count == 0) {
        return SYS_ERR_NOT_FOUND;
    }

    memcpy(out, queue->storage + (queue->head * queue->item_size),
           queue->item_size);
    return SYS_OK;
}

size_t event_queue_count(const event_queue_t *queue)
{
    return (queue == NULL) ? 0 : queue->count;
}

size_t event_queue_capacity(const event_queue_t *queue)
{
    return (queue == NULL) ? 0 : queue->capacity;
}

bool event_queue_is_empty(const event_queue_t *queue)
{
    return event_queue_count(queue) == 0;
}

void event_queue_clear(event_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }
    queue->head  = 0;
    queue->count = 0;
}

#endif /* BOARD_ESP32S3 / BOARD_SIMULATOR */
