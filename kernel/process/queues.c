/*
 * kernel/process/queues.c
 *
 * JARVIS OS — fixed-capacity PID queues.
 */

#include "queues.h"

void queue_init(jvk_queue_t* q)
{
    q->count = 0;
}

int queue_push(jvk_queue_t* q, int pid)
{
    if (q->count >= JVK_QUEUE_CAP) {
        return 0;
    }
    if (queue_contains(q, pid)) {
        return 0;
    }
    q->items[q->count++] = pid;
    return 1;
}

int queue_remove(jvk_queue_t* q, int pid)
{
    for (int i = 0; i < q->count; i++) {
        if (q->items[i] == pid) {
            for (int j = i; j < q->count - 1; j++) {
                q->items[j] = q->items[j + 1];
            }
            q->count--;
            return 1;
        }
    }
    return 0;
}

int queue_contains(const jvk_queue_t* q, int pid)
{
    for (int i = 0; i < q->count; i++) {
        if (q->items[i] == pid) {
            return 1;
        }
    }
    return 0;
}
