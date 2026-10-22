/*
 * kernel/ipc/ipc_manager.h
 *
 * JARVIS OS — IPC authority (M7).
 *
 * The manager owns every IPC object: message queues, shared-memory
 * segments, pipes, mutexes and semaphores. Subsystems above it (the
 * kernel glue) translate JSON, move processes between READY/WAITING and
 * publish snapshot data; the manager never calls the process manager or
 * the scheduler directly (Kernel → Subsystem, never Subsystem → Subsystem).
 *
 * Blocking contract:
 *   - an operation that cannot proceed without waiting returns
 *     JVK_IPC_BLOCKED and enqueues the caller;
 *   - every transition that unblocks somebody records that pid in the
 *     manager's wake list, drained with ipc_take_wakeups();
 *   - ipc_release_pid() removes every trace of a terminated process and
 *     wakes whoever depended on it.
 */

#ifndef JARVIS_IPC_MANAGER_H
#define JARVIS_IPC_MANAGER_H

#include "ipc_types.h"

/* ---- object records ---------------------------------------------------- */

typedef struct {
    int  pid;
    char data[JVK_IPC_MSG_LEN];
} jvk_ipc_msg_waiter_t;

typedef struct {
    int  used;
    char name[JVK_IPC_NAME_LEN];
    int  owner;                 /* creator pid, 0 = system */
    int  capacity;              /* <= JVK_IPC_QUEUE_CAP */
    char msgs[JVK_IPC_QUEUE_CAP][JVK_IPC_MSG_LEN];
    int  head;
    int  tail;
    int  count;
    jvk_ipc_msg_waiter_t senders[JVK_IPC_MAX_WAITERS]; /* blocked senders */
    int  sender_count;
    int  receivers[JVK_IPC_MAX_WAITERS];               /* blocked receivers */
    int  receiver_count;
    unsigned long created_ticks;
    unsigned long sent;
    unsigned long received;
    unsigned long blocked_sends;
    unsigned long blocked_recvs;
} jvk_ipc_queue_t;

typedef struct {
    int  used;
    char name[JVK_IPC_NAME_LEN];
    int  owner;
    int  size;                  /* requested bytes, <= JVK_IPC_SEG_LEN */
    int  length;                /* bytes actually written */
    char data[JVK_IPC_SEG_LEN];
    int  attached[JVK_IPC_MAX_WAITERS];
    int  attach_count;
    unsigned long created_ticks;
    unsigned long maps;
    unsigned long writes;
    unsigned long reads;
} jvk_ipc_segment_t;

typedef struct {
    int  pid;
    char data[JVK_IPC_PIPE_CHUNK];
    int  len;
    int  off;
} jvk_ipc_pipe_writer_t;

typedef struct {
    int  pid;
    int  max;
} jvk_ipc_pipe_reader_t;

typedef struct {
    int  used;
    char name[JVK_IPC_NAME_LEN];
    int  owner;
    int  capacity;              /* <= JVK_IPC_PIPE_LEN */
    char buf[JVK_IPC_PIPE_LEN];
    int  head;
    int  tail;
    int  count;
    int  readers[JVK_IPC_MAX_WAITERS];
    int  reader_count;
    int  writers[JVK_IPC_MAX_WAITERS];
    int  writer_count;
    jvk_ipc_pipe_reader_t read_waiters[JVK_IPC_MAX_WAITERS];
    int  read_wait_count;
    jvk_ipc_pipe_writer_t write_waiters[JVK_IPC_MAX_WAITERS];
    int  write_wait_count;
    unsigned long created_ticks;
    unsigned long reads;
    unsigned long writes;
    unsigned long blocked_reads;
    unsigned long blocked_writes;
    unsigned long bytes_in;
    unsigned long bytes_out;
} jvk_ipc_pipe_t;

typedef struct {
    int  used;
    char name[JVK_IPC_NAME_LEN];
    int  owner;                 /* 0 = free */
    int  recursion;             /* recursive lock depth */
    int  waiters[JVK_IPC_MAX_WAITERS];
    int  waiter_count;
    unsigned long created_ticks;
    unsigned long locks;
    unsigned long unlocks;
    unsigned long contentions;
} jvk_ipc_mutex_t;

typedef struct {
    int  used;
    char name[JVK_IPC_NAME_LEN];
    int  owner;
    int  value;                 /* available permits */
    int  max;
    int  holders[JVK_IPC_MAX_WAITERS]; /* pids holding permits */
    int  holder_count;
    int  waiters[JVK_IPC_MAX_WAITERS];
    int  waiter_count;
    unsigned long created_ticks;
    unsigned long waits;
    unsigned long posts;
    unsigned long contentions;
} jvk_ipc_sem_t;

typedef struct {
    int  pid;
    int  valid;
    int  len;
    char data[JVK_IPC_PIPE_LEN];
} jvk_ipc_delivery_t;

typedef struct {
    unsigned long queues_created;
    unsigned long messages_sent;
    unsigned long messages_received;
    unsigned long messages_dropped;
    unsigned long segments_created;
    unsigned long shm_maps;
    unsigned long shm_writes;
    unsigned long shm_reads;
    unsigned long pipes_created;
    unsigned long pipe_reads;
    unsigned long pipe_writes;
    unsigned long pipe_bytes;
    unsigned long mutex_locks;
    unsigned long mutex_unlocks;
    unsigned long mutex_contentions;
    unsigned long sem_waits;
    unsigned long sem_posts;
    unsigned long sem_contentions;
    unsigned long blocked;
    unsigned long woken;
    unsigned long deadlocks;
    unsigned long recoveries;
} jvk_ipc_stats_t;

typedef void (*jvk_ipc_event_fn)(void* user, const char* message);

typedef struct {
    jvk_ipc_queue_t    queues[JVK_IPC_MAX_QUEUES];
    jvk_ipc_segment_t  segments[JVK_IPC_MAX_SEGMENTS];
    jvk_ipc_pipe_t     pipes[JVK_IPC_MAX_PIPES];
    jvk_ipc_mutex_t    mutexes[JVK_IPC_MAX_MUTEXES];
    jvk_ipc_sem_t      sems[JVK_IPC_MAX_SEMS];
    jvk_ipc_delivery_t deliveries[JVK_IPC_MAX_PROCS];
    int                wake[JVK_IPC_MAX_PROCS];
    int                wake_count;
    jvk_ipc_stats_t    stats;
    unsigned long      deadlock_signature;
    int                deadlock_active;
    /* boot configuration (config/ipc.json, all optional) */
    int                cfg_queue_capacity;
    int                cfg_pipe_capacity;
    int                cfg_shm_bytes;
    jvk_ipc_event_fn   on_event;
    void*              event_user;
} jvk_ipc_t;

/* ---- lifecycle --------------------------------------------------------- */

void ipc_init(jvk_ipc_t* ipc);
void ipc_shutdown(jvk_ipc_t* ipc);
void ipc_set_observer(jvk_ipc_t* ipc, jvk_ipc_event_fn fn, void* user);

/* ---- message queues ---------------------------------------------------- */

jvk_ipc_status_t ipc_queue_create(jvk_ipc_t* ipc, const char* name, int owner,
                                  int capacity, unsigned long ticks);
jvk_ipc_status_t ipc_queue_send(jvk_ipc_t* ipc, const char* name, int pid,
                                const char* data, int nonblock);
jvk_ipc_status_t ipc_queue_recv(jvk_ipc_t* ipc, const char* name, int pid,
                                int nonblock, char* out, int cap, int* out_len);
jvk_ipc_status_t ipc_queue_destroy(jvk_ipc_t* ipc, const char* name, int pid);

/* A blocked receiver gets its message handed over on wakeup. */
jvk_ipc_status_t ipc_take_delivery(jvk_ipc_t* ipc, int pid,
                                   char* out, int cap, int* out_len);

/* ---- shared memory ----------------------------------------------------- */

jvk_ipc_status_t ipc_shm_create(jvk_ipc_t* ipc, const char* name, int pid,
                                int size, unsigned long ticks);
jvk_ipc_status_t ipc_shm_attach(jvk_ipc_t* ipc, const char* name, int pid);
jvk_ipc_status_t ipc_shm_detach(jvk_ipc_t* ipc, const char* name, int pid);
jvk_ipc_status_t ipc_shm_write(jvk_ipc_t* ipc, const char* name, int pid,
                               const char* data, int offset, int* written);
jvk_ipc_status_t ipc_shm_read(jvk_ipc_t* ipc, const char* name, int pid,
                              char* out, int cap, int* out_len);
jvk_ipc_status_t ipc_shm_destroy(jvk_ipc_t* ipc, const char* name, int pid);

/* ---- pipes ------------------------------------------------------------- */

jvk_ipc_status_t ipc_pipe_create(jvk_ipc_t* ipc, const char* name, int owner,
                                 int capacity, unsigned long ticks);
jvk_ipc_status_t ipc_pipe_open(jvk_ipc_t* ipc, const char* name, int pid,
                               const char* mode);
jvk_ipc_status_t ipc_pipe_close(jvk_ipc_t* ipc, const char* name, int pid);
jvk_ipc_status_t ipc_pipe_write(jvk_ipc_t* ipc, const char* name, int pid,
                                const char* data, int nonblock, int* written);
jvk_ipc_status_t ipc_pipe_read(jvk_ipc_t* ipc, const char* name, int pid,
                               int max, int nonblock,
                               char* out, int cap, int* out_len);
jvk_ipc_status_t ipc_pipe_destroy(jvk_ipc_t* ipc, const char* name, int pid);

/* ---- mutexes & semaphores --------------------------------------------- */

jvk_ipc_status_t ipc_mutex_create(jvk_ipc_t* ipc, const char* name, int pid,
                                  unsigned long ticks);
jvk_ipc_status_t ipc_mutex_lock(jvk_ipc_t* ipc, const char* name, int pid,
                                int nonblock);
jvk_ipc_status_t ipc_mutex_unlock(jvk_ipc_t* ipc, const char* name, int pid);
jvk_ipc_status_t ipc_mutex_destroy(jvk_ipc_t* ipc, const char* name, int pid);

jvk_ipc_status_t ipc_sem_create(jvk_ipc_t* ipc, const char* name, int pid,
                                int value, int max, unsigned long ticks);
jvk_ipc_status_t ipc_sem_wait(jvk_ipc_t* ipc, const char* name, int pid,
                              int nonblock);
jvk_ipc_status_t ipc_sem_post(jvk_ipc_t* ipc, const char* name, int pid);
jvk_ipc_status_t ipc_sem_destroy(jvk_ipc_t* ipc, const char* name, int pid);

/* ---- blocking / cleanup ----------------------------------------------- */

int  ipc_is_waiting(const jvk_ipc_t* ipc, int pid);

/* Drain the pids unblocked since the last call. Returns the count. */
int  ipc_take_wakeups(jvk_ipc_t* ipc, int* out, int cap);

/* Remove every trace of a terminated process (waits, locks, endpoints,
   attachments, pending deliveries). Wakes dependent processes. */
void ipc_release_pid(jvk_ipc_t* ipc, int pid, unsigned long ticks);

/* ---- deadlock detection ------------------------------------------------ */

typedef struct {
    int         pid;      /* waiting process */
    int         target;   /* process that must release the resource */
    const char* kind;     /* "mutex" | "semaphore" */
    const char* resource; /* object name */
} jvk_ipc_edge_t;

typedef struct {
    int pids[JVK_IPC_MAX_PROCS];
    int count;
} jvk_ipc_cycle_t;

/* Build the wait-for graph (deduplicated). Returns the edge count. */
int ipc_wait_edges(const jvk_ipc_t* ipc, jvk_ipc_edge_t* edges, int cap);

/* Find cycles in the wait-for graph. Returns the cycle count and updates
   the manager's deadlock counters/signature. */
int ipc_detect_deadlock(jvk_ipc_t* ipc, jvk_ipc_cycle_t* cycles, int cap);

#endif /* JARVIS_IPC_MANAGER_H */
