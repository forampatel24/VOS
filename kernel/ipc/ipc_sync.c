/*
 * kernel/ipc/ipc_sync.c
 *
 * JARVIS OS — mutexes and counting semaphores (M7).
 *
 * Mutexes are recursive for their owner and hand ownership directly to
 * the first waiter on unlock (no thundering herd). Semaphores are
 * counting; a post during contention transfers the permit straight to
 * the first waiter. Both keep honest waiter lists so the deadlock
 * detector can build a wait-for graph.
 */

#include <stdio.h>
#include <string.h>

#include "ipc_internal.h"

void ipc_sem_add_holder(jvk_ipc_sem_t* sem, int pid)
{
    if (sem->holder_count < JVK_IPC_MAX_WAITERS) {
        sem->holders[sem->holder_count++] = pid;
    }
}

void ipc_sem_remove_holder(jvk_ipc_sem_t* sem, int pid)
{
    (void)ipc_array_remove(sem->holders, &sem->holder_count, pid);
}

static jvk_ipc_mutex_t* find_mutex(jvk_ipc_t* ipc, const char* name)
{
    if (ipc == NULL || name == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_IPC_MAX_MUTEXES; i++) {
        if (ipc->mutexes[i].used &&
            strcmp(ipc->mutexes[i].name, name) == 0) {
            return &ipc->mutexes[i];
        }
    }
    return NULL;
}

static jvk_ipc_sem_t* find_sem(jvk_ipc_t* ipc, const char* name)
{
    if (ipc == NULL || name == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_IPC_MAX_SEMS; i++) {
        if (ipc->sems[i].used && strcmp(ipc->sems[i].name, name) == 0) {
            return &ipc->sems[i];
        }
    }
    return NULL;
}

/* ---- mutexes ----------------------------------------------------------- */

jvk_ipc_status_t ipc_mutex_create(jvk_ipc_t* ipc, const char* name, int pid,
                                  unsigned long ticks)
{
    (void)pid;
    if (ipc == NULL || name == NULL || name[0] == '\0' ||
        strlen(name) >= JVK_IPC_NAME_LEN) {
        return JVK_IPC_ERR_ARG;
    }
    if (find_mutex(ipc, name) != NULL) {
        return JVK_IPC_ERR_EXIST;
    }
    for (int i = 0; i < JVK_IPC_MAX_MUTEXES; i++) {
        jvk_ipc_mutex_t* m = &ipc->mutexes[i];
        if (m->used) {
            continue;
        }
        memset(m, 0, sizeof(*m));
        m->used = 1;
        snprintf(m->name, sizeof(m->name), "%s", name);
        m->created_ticks = ticks;
        ipc_emit(ipc, "IPC_MUTEX_CREATED name=%s", name);
        return JVK_IPC_OK;
    }
    return JVK_IPC_ERR_FULL;
}

jvk_ipc_status_t ipc_mutex_lock(jvk_ipc_t* ipc, const char* name, int pid,
                                int nonblock)
{
    jvk_ipc_mutex_t* m = find_mutex(ipc, name);
    if (m == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (m->owner == pid) {
        m->recursion++;
        m->locks++;
        ipc->stats.mutex_locks++;
        return JVK_IPC_OK;
    }
    if (m->owner == 0) {
        m->owner = pid;
        m->recursion = 1;
        m->locks++;
        ipc->stats.mutex_locks++;
        ipc_emit(ipc, "IPC_MUTEX_LOCK name=%s pid=%d", m->name, pid);
        return JVK_IPC_OK;
    }
    m->contentions++;
    ipc->stats.mutex_contentions++;
    if (nonblock) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (pid <= 0) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (ipc_array_contains(m->waiters, m->waiter_count, pid)) {
        return JVK_IPC_ERR_BUSY;
    }
    if (m->waiter_count >= JVK_IPC_MAX_WAITERS) {
        return JVK_IPC_ERR_FULL;
    }
    m->waiters[m->waiter_count++] = pid;
    ipc->stats.blocked++;
    ipc_emit(ipc, "IPC_BLOCK name=%s pid=%d kind=mutex", m->name, pid);
    return JVK_IPC_BLOCKED;
}

jvk_ipc_status_t ipc_mutex_unlock(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_mutex_t* m = find_mutex(ipc, name);
    if (m == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (m->owner != pid) {
        return JVK_IPC_ERR_PERM;
    }
    if (m->recursion > 1) {
        m->recursion--;
        m->unlocks++;
        ipc->stats.mutex_unlocks++;
        return JVK_IPC_OK;
    }
    m->recursion = 0;
    m->unlocks++;
    ipc->stats.mutex_unlocks++;
    if (m->waiter_count > 0) {
        int next = m->waiters[0];
        ipc_array_remove(m->waiters, &m->waiter_count, next);
        m->owner = next;
        m->recursion = 1;
        m->locks++;
        ipc->stats.mutex_locks++;
        ipc_wake(ipc, next);
        ipc_emit(ipc, "IPC_MUTEX_HANDOFF name=%s from=%d to=%d",
                 m->name, pid, next);
    } else {
        m->owner = 0;
        ipc_emit(ipc, "IPC_MUTEX_UNLOCK name=%s pid=%d", m->name, pid);
    }
    return JVK_IPC_OK;
}

jvk_ipc_status_t ipc_mutex_destroy(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_mutex_t* m = find_mutex(ipc, name);
    if (m == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (m->owner != 0 && m->owner != pid && pid != 0) {
        return JVK_IPC_ERR_PERM;
    }
    for (int i = 0; i < m->waiter_count; i++) {
        ipc_wake(ipc, m->waiters[i]);
    }
    ipc_emit(ipc, "IPC_MUTEX_DESTROYED name=%s pid=%d", m->name, pid);
    memset(m, 0, sizeof(*m));
    return JVK_IPC_OK;
}

/* ---- semaphores -------------------------------------------------------- */

jvk_ipc_status_t ipc_sem_create(jvk_ipc_t* ipc, const char* name, int pid,
                                int value, int max, unsigned long ticks)
{
    if (ipc == NULL || name == NULL || name[0] == '\0' ||
        strlen(name) >= JVK_IPC_NAME_LEN) {
        return JVK_IPC_ERR_ARG;
    }
    if (value < 0 || max < 0) {
        return JVK_IPC_ERR_ARG;
    }
    if (find_sem(ipc, name) != NULL) {
        return JVK_IPC_ERR_EXIST;
    }
    if (max == 0) {
        max = value > 0 ? value : 4;
    }
    if (max > JVK_IPC_MAX_WAITERS) {
        max = JVK_IPC_MAX_WAITERS;
    }
    if (value > max) {
        value = max;
    }
    for (int i = 0; i < JVK_IPC_MAX_SEMS; i++) {
        jvk_ipc_sem_t* s = &ipc->sems[i];
        if (s->used) {
            continue;
        }
        memset(s, 0, sizeof(*s));
        s->used = 1;
        snprintf(s->name, sizeof(s->name), "%s", name);
        s->owner = pid;
        s->value = value;
        s->max = max;
        s->created_ticks = ticks;
        ipc_emit(ipc, "IPC_SEM_CREATED name=%s value=%d max=%d",
                 name, value, max);
        return JVK_IPC_OK;
    }
    return JVK_IPC_ERR_FULL;
}

jvk_ipc_status_t ipc_sem_wait(jvk_ipc_t* ipc, const char* name, int pid,
                              int nonblock)
{
    jvk_ipc_sem_t* s = find_sem(ipc, name);
    if (s == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (s->value > 0) {
        s->value--;
        ipc_sem_add_holder(s, pid);
        s->waits++;
        ipc->stats.sem_waits++;
        ipc_emit(ipc, "IPC_SEM_WAIT name=%s pid=%d value=%d",
                 s->name, pid, s->value);
        return JVK_IPC_OK;
    }
    s->contentions++;
    ipc->stats.sem_contentions++;
    if (nonblock) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (pid <= 0) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (ipc_array_contains(s->waiters, s->waiter_count, pid)) {
        return JVK_IPC_ERR_BUSY;
    }
    if (s->waiter_count >= JVK_IPC_MAX_WAITERS) {
        return JVK_IPC_ERR_FULL;
    }
    s->waiters[s->waiter_count++] = pid;
    ipc->stats.blocked++;
    ipc_emit(ipc, "IPC_BLOCK name=%s pid=%d kind=semaphore", s->name, pid);
    return JVK_IPC_BLOCKED;
}

jvk_ipc_status_t ipc_sem_post(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_sem_t* s = find_sem(ipc, name);
    if (s == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (s->waiter_count > 0) {
        int next = s->waiters[0];
        ipc_array_remove(s->waiters, &s->waiter_count, next);
        /* The poster's permit (or a fresh one) goes straight to the waiter. */
        if (ipc_array_contains(s->holders, s->holder_count, pid)) {
            ipc_sem_remove_holder(s, pid);
        }
        ipc_sem_add_holder(s, next);
        s->posts++;
        ipc->stats.sem_posts++;
        ipc_wake(ipc, next);
        ipc_emit(ipc, "IPC_SEM_HANDOFF name=%s from=%d to=%d",
                 s->name, pid, next);
        return JVK_IPC_OK;
    }
    if (s->value >= s->max) {
        return JVK_IPC_ERR_FULL;
    }
    s->value++;
    ipc_sem_remove_holder(s, pid);
    s->posts++;
    ipc->stats.sem_posts++;
    ipc_emit(ipc, "IPC_SEM_POST name=%s pid=%d value=%d",
             s->name, pid, s->value);
    return JVK_IPC_OK;
}

jvk_ipc_status_t ipc_sem_destroy(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_sem_t* s = find_sem(ipc, name);
    if (s == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (pid != 0 && s->owner != pid && s->owner != 0) {
        return JVK_IPC_ERR_PERM;
    }
    for (int i = 0; i < s->waiter_count; i++) {
        ipc_wake(ipc, s->waiters[i]);
    }
    ipc_emit(ipc, "IPC_SEM_DESTROYED name=%s pid=%d", s->name, pid);
    memset(s, 0, sizeof(*s));
    return JVK_IPC_OK;
}
