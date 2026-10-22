/*
 * kernel/ipc/ipc_types.h
 *
 * JARVIS OS — shared IPC types, capacities and status codes.
 *
 * M7 provides four classic inter-process communication mechanisms plus
 * their synchronisation primitives:
 *
 *   message queues   bounded FIFO of small messages, name-addressed
 *   shared memory    named byte segments, attach/detach per process
 *   pipes            bounded byte stream with reader/writer endpoints
 *   mutex/semaphore  ownership + counting semaphores, blocking waiters
 *
 * Every blocking path is modelled honestly: the caller is queued in the
 * resource, reported as BLOCKED to the kernel glue, and woken when the
 * resource changes state. Deadlock detection walks the resulting
 * wait-for graph.
 */

#ifndef JARVIS_IPC_TYPES_H
#define JARVIS_IPC_TYPES_H

#define JVK_IPC_NAME_LEN     24
#define JVK_IPC_MAX_PROCS     8

/* message queues */
#define JVK_IPC_MAX_QUEUES    6
#define JVK_IPC_QUEUE_CAP    16
#define JVK_IPC_MSG_LEN      96

/* shared memory */
#define JVK_IPC_MAX_SEGMENTS  6
#define JVK_IPC_SEG_LEN     512

/* pipes */
#define JVK_IPC_MAX_PIPES     6
#define JVK_IPC_PIPE_LEN    512
#define JVK_IPC_PIPE_CHUNK  128

/* synchronisation */
#define JVK_IPC_MAX_MUTEXES   8
#define JVK_IPC_MAX_SEMS      8

#define JVK_IPC_MAX_WAITERS   8
#define JVK_IPC_MAX_CYCLES    8
#define JVK_IPC_MAX_EDGES    24

typedef enum {
    JVK_IPC_OK = 0,
    JVK_IPC_BLOCKED,      /* caller queued on the resource (not an error) */
    JVK_IPC_ERR_ARG,
    JVK_IPC_ERR_NOENT,
    JVK_IPC_ERR_EXIST,
    JVK_IPC_ERR_FULL,
    JVK_IPC_ERR_EMPTY,
    JVK_IPC_ERR_AGAIN,    /* would block, non-blocking request */
    JVK_IPC_ERR_BUSY,     /* caller already waiting on this resource */
    JVK_IPC_ERR_PERM,
    JVK_IPC_ERR_DEADLK,
    JVK_IPC_ERR_COUNT,
} jvk_ipc_status_t;

const char* jvk_ipc_status_name(jvk_ipc_status_t st);

#endif /* JARVIS_IPC_TYPES_H */
