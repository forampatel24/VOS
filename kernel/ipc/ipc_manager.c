/*
 * kernel/ipc/ipc_manager.c
 *
 * JARVIS OS — IPC manager lifecycle, wake bookkeeping and cleanup.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ipc_internal.h"

void ipc_emit(jvk_ipc_t* ipc, const char* fmt, ...)
{
    if (ipc == NULL || ipc->on_event == NULL || fmt == NULL) {
        return;
    }
    char msg[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    ipc->on_event(ipc->event_user, msg);
}

void ipc_wake(jvk_ipc_t* ipc, int pid)
{
    if (ipc == NULL || pid <= 0) {
        return;
    }
    for (int i = 0; i < ipc->wake_count; i++) {
        if (ipc->wake[i] == pid) {
            return;
        }
    }
    if (ipc->wake_count >= JVK_IPC_MAX_PROCS) {
        return;
    }
    ipc->wake[ipc->wake_count++] = pid;
    ipc->stats.woken++;
}

int ipc_take_wakeups(jvk_ipc_t* ipc, int* out, int cap)
{
    if (ipc == NULL || out == NULL || cap <= 0) {
        return 0;
    }
    int n = ipc->wake_count < cap ? ipc->wake_count : cap;
    for (int i = 0; i < n; i++) {
        out[i] = ipc->wake[i];
    }
    ipc->wake_count = 0;
    return n;
}

int ipc_array_remove(int* arr, int* count, int value)
{
    if (arr == NULL || count == NULL) {
        return 0;
    }
    for (int i = 0; i < *count; i++) {
        if (arr[i] == value) {
            for (int j = i; j < *count - 1; j++) {
                arr[j] = arr[j + 1];
            }
            (*count)--;
            return 1;
        }
    }
    return 0;
}

int ipc_array_contains(const int* arr, int count, int value)
{
    for (int i = 0; i < count; i++) {
        if (arr[i] == value) {
            return 1;
        }
    }
    return 0;
}

jvk_ipc_delivery_t* ipc_delivery_slot(jvk_ipc_t* ipc, int pid, int create)
{
    if (ipc == NULL || pid <= 0) {
        return NULL;
    }
    for (int i = 0; i < JVK_IPC_MAX_PROCS; i++) {
        if (ipc->deliveries[i].valid && ipc->deliveries[i].pid == pid) {
            return &ipc->deliveries[i];
        }
    }
    if (!create) {
        return NULL;
    }
    for (int i = 0; i < JVK_IPC_MAX_PROCS; i++) {
        if (!ipc->deliveries[i].valid) {
            ipc->deliveries[i].valid = 1;
            ipc->deliveries[i].pid = pid;
            ipc->deliveries[i].len = 0;
            ipc->deliveries[i].data[0] = '\0';
            return &ipc->deliveries[i];
        }
    }
    return NULL;
}

int ipc_delivery_push(jvk_ipc_t* ipc, int pid, const char* data, int len)
{
    jvk_ipc_delivery_t* slot = ipc_delivery_slot(ipc, pid, 1);
    if (slot == NULL) {
        return 0;
    }
    if (len < 0) {
        len = 0;
    }
    if (len >= (int)sizeof(slot->data)) {
        len = (int)sizeof(slot->data) - 1;
    }
    if (len > 0 && data != NULL) {
        memcpy(slot->data, data, (size_t)len);
    }
    slot->data[len] = '\0';
    slot->len = len;
    return 1;
}

void ipc_init(jvk_ipc_t* ipc)
{
    if (ipc == NULL) {
        return;
    }
    memset(ipc, 0, sizeof(*ipc));
}

void ipc_shutdown(jvk_ipc_t* ipc)
{
    if (ipc == NULL) {
        return;
    }
    memset(ipc, 0, sizeof(*ipc));
}

void ipc_set_observer(jvk_ipc_t* ipc, jvk_ipc_event_fn fn, void* user)
{
    if (ipc == NULL) {
        return;
    }
    ipc->on_event = fn;
    ipc->event_user = user;
}

int ipc_is_waiting(const jvk_ipc_t* ipc, int pid)
{
    if (ipc == NULL || pid <= 0) {
        return 0;
    }
    for (int i = 0; i < JVK_IPC_MAX_QUEUES; i++) {
        const jvk_ipc_queue_t* q = &ipc->queues[i];
        if (!q->used) {
            continue;
        }
        for (int w = 0; w < q->sender_count; w++) {
            if (q->senders[w].pid == pid) {
                return 1;
            }
        }
        if (ipc_array_contains(q->receivers, q->receiver_count, pid)) {
            return 1;
        }
    }
    for (int i = 0; i < JVK_IPC_MAX_PIPES; i++) {
        const jvk_ipc_pipe_t* p = &ipc->pipes[i];
        if (!p->used) {
            continue;
        }
        for (int w = 0; w < p->read_wait_count; w++) {
            if (p->read_waiters[w].pid == pid) {
                return 1;
            }
        }
        for (int w = 0; w < p->write_wait_count; w++) {
            if (p->write_waiters[w].pid == pid) {
                return 1;
            }
        }
    }
    for (int i = 0; i < JVK_IPC_MAX_MUTEXES; i++) {
        const jvk_ipc_mutex_t* m = &ipc->mutexes[i];
        if (m->used && ipc_array_contains(m->waiters, m->waiter_count, pid)) {
            return 1;
        }
    }
    for (int i = 0; i < JVK_IPC_MAX_SEMS; i++) {
        const jvk_ipc_sem_t* s = &ipc->sems[i];
        if (s->used && ipc_array_contains(s->waiters, s->waiter_count, pid)) {
            return 1;
        }
    }
    return 0;
}

/* Mutex handoff: give ownership to the first waiter and wake it. */
static void release_mutex(jvk_ipc_mutex_t* m, int pid, jvk_ipc_t* ipc)
{
    if (m->owner != pid) {
        return;
    }
    if (m->waiter_count > 0) {
        int next = m->waiters[0];
        ipc_array_remove(m->waiters, &m->waiter_count, next);
        m->owner = next;
        m->recursion = 1;
        ipc_wake(ipc, next);
    } else {
        m->owner = 0;
        m->recursion = 0;
    }
}

/* Give back every permit the dead process held, waking waiters in order. */
static void release_sem(jvk_ipc_sem_t* s, int pid, jvk_ipc_t* ipc)
{
    while (ipc_array_remove(s->holders, &s->holder_count, pid)) {
        if (s->value < s->max) {
            s->value++;
        }
    }
    while (s->value > 0 && s->waiter_count > 0) {
        int next = s->waiters[0];
        ipc_array_remove(s->waiters, &s->waiter_count, next);
        s->value--;
        ipc_sem_add_holder(s, next);
        ipc_wake(ipc, next);
    }
}

void ipc_release_pid(jvk_ipc_t* ipc, int pid, unsigned long ticks)
{
    (void)ticks;
    if (ipc == NULL || pid <= 0) {
        return;
    }

    for (int i = 0; i < JVK_IPC_MAX_PROCS; i++) {
        if (ipc->deliveries[i].valid && ipc->deliveries[i].pid == pid) {
            ipc->deliveries[i].valid = 0;
            ipc->deliveries[i].len = 0;
        }
    }

    for (int i = 0; i < JVK_IPC_MAX_QUEUES; i++) {
        jvk_ipc_queue_t* q = &ipc->queues[i];
        if (!q->used) {
            continue;
        }
        if (q->owner == pid) {
            q->owner = 0;
        }
        ipc_array_remove(q->receivers, &q->receiver_count, pid);
        for (int w = 0; w < q->sender_count;) {
            if (q->senders[w].pid == pid) {
                for (int j = w; j < q->sender_count - 1; j++) {
                    q->senders[j] = q->senders[j + 1];
                }
                q->sender_count--;
                ipc->stats.messages_dropped++;
            } else {
                w++;
            }
        }
    }

    for (int i = 0; i < JVK_IPC_MAX_SEGMENTS; i++) {
        jvk_ipc_segment_t* seg = &ipc->segments[i];
        if (!seg->used) {
            continue;
        }
        ipc_array_remove(seg->attached, &seg->attach_count, pid);
        if (seg->owner == pid) {
            seg->owner = 0;
        }
    }

    for (int i = 0; i < JVK_IPC_MAX_PIPES; i++) {
        jvk_ipc_pipe_t* p = &ipc->pipes[i];
        if (!p->used) {
            continue;
        }
        if (p->owner == pid) {
            p->owner = 0;
        }
        ipc_array_remove(p->readers, &p->reader_count, pid);
        ipc_array_remove(p->writers, &p->writer_count, pid);
        for (int w = 0; w < p->read_wait_count;) {
            if (p->read_waiters[w].pid == pid) {
                for (int j = w; j < p->read_wait_count - 1; j++) {
                    p->read_waiters[j] = p->read_waiters[j + 1];
                }
                p->read_wait_count--;
            } else {
                w++;
            }
        }
        for (int w = 0; w < p->write_wait_count;) {
            if (p->write_waiters[w].pid == pid) {
                for (int j = w; j < p->write_wait_count - 1; j++) {
                    p->write_waiters[j] = p->write_waiters[j + 1];
                }
                p->write_wait_count--;
            } else {
                w++;
            }
        }
    }

    for (int i = 0; i < JVK_IPC_MAX_MUTEXES; i++) {
        jvk_ipc_mutex_t* m = &ipc->mutexes[i];
        if (!m->used) {
            continue;
        }
        ipc_array_remove(m->waiters, &m->waiter_count, pid);
        if (m->owner == pid) {
            release_mutex(m, pid, ipc);
        }
    }

    for (int i = 0; i < JVK_IPC_MAX_SEMS; i++) {
        jvk_ipc_sem_t* s = &ipc->sems[i];
        if (!s->used) {
            continue;
        }
        ipc_array_remove(s->waiters, &s->waiter_count, pid);
        release_sem(s, pid, ipc);
    }
}
