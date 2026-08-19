/*
 * kernel/process/queues.h
 *
 * JARVIS OS — fixed-capacity PID queues.
 *
 * The scheduler keeps ready / waiting / suspended / terminated queues as
 * simple arrays of PIDs. Operations are O(n) which is fine for the tiny
 * simulated process table (JVK_MAX_PROCS).
 */

#ifndef JARVIS_QUEUES_H
#define JARVIS_QUEUES_H

#define JVK_QUEUE_CAP 16

typedef struct {
    int items[JVK_QUEUE_CAP];
    int count;
} jvk_queue_t;

void queue_init(jvk_queue_t* q);
int  queue_push(jvk_queue_t* q, int pid);       /* 1 ok, 0 full/dup */
int  queue_remove(jvk_queue_t* q, int pid);     /* 1 removed, 0 absent */
int  queue_contains(const jvk_queue_t* q, int pid);

#endif /* JARVIS_QUEUES_H */
