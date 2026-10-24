/*
 * event_queue.h — cross-platform event queue.
 *
 * One queue abstraction, two implementations:
 *
 *   - On the ESP32-S3 the queue is a FreeRTOS queue, so a producer on
 *     one task and a consumer on another are safe without any extra
 *     locking.
 *   - On the host simulator the queue is a single-threaded ring
 *     buffer, which is all the simulator's single event loop needs.
 *
 * The queue type is opaque: callers hold a pointer to it, and the
 * header exposes no FreeRTOS type. That keeps the interface usable
 * from plain C on the host and lets the IDE index it cleanly.
 *
 * The queue stores fixed-size items and never allocates after
 * creation. Pushes on a full queue drop the newest item and report
 * SYS_ERR_BUSY rather than overwriting an item that has not been
 * consumed yet.
 */
#ifndef SYNTH_EVENT_QUEUE_H
#define SYNTH_EVENT_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "common/sys_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque queue handle. */
typedef struct event_queue event_queue_t;

/**
 * Create a queue.
 *
 * @param item_size    Size of one item in bytes; must be non-zero.
 * @param capacity     Maximum number of items the queue can hold.
 * @param out_queue    Receives the new queue. Must not be NULL.
 *
 * The queue is allocated on the heap. On the firmware the underlying
 * FreeRTOS queue is allocated from the heap the caller is running on.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, or SYS_ERR_NO_MEM.
 */
sys_status_t event_queue_create(size_t item_size,
                                size_t capacity,
                                event_queue_t **out_queue);

/**
 * Destroy a queue. Safe to call with NULL.
 */
void event_queue_destroy(event_queue_t *queue);

/**
 * Append one item to the tail of the queue.
 *
 * Returns SYS_OK, SYS_ERR_INVALID_ARG, or SYS_ERR_BUSY when the queue
 * is full.
 */
sys_status_t event_queue_push(event_queue_t *queue, const void *item);

/**
 * Remove the item at the head of the queue.
 *
 * @param queue The queue.
 * @param out   Receives the item. Must not be NULL.
 *
 * Returns SYS_OK, or SYS_ERR_NOT_FOUND when the queue is empty.
 */
sys_status_t event_queue_pop(event_queue_t *queue, void *out);

/**
 * Look at the head of the queue without removing it.
 *
 * Returns SYS_OK, or SYS_ERR_NOT_FOUND when the queue is empty.
 */
sys_status_t event_queue_peek(const event_queue_t *queue, void *out);

/**
 * @return the number of items currently queued.
 */
size_t event_queue_count(const event_queue_t *queue);

/**
 * @return the maximum number of items the queue can hold.
 */
size_t event_queue_capacity(const event_queue_t *queue);

/**
 * @return true when the queue holds no items.
 */
bool event_queue_is_empty(const event_queue_t *queue);

/**
 * Discard every queued item. The queue remains usable.
 */
void event_queue_clear(event_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* SYNTH_EVENT_QUEUE_H */
