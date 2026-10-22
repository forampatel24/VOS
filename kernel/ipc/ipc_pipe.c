/*
 * kernel/ipc/ipc_pipe.c
 *
 * JARVIS OS — bounded byte pipes (M7).
 *
 * A named pipe has reader and writer endpoints. The creator is attached
 * to both ends so a single process can exercise the pipe from the demo.
 * Writers block when the ring is full (their chunk is held in the wait
 * record and committed as space frees); readers block when it is empty
 * and receive the next chunk straight into their delivery slot.
 */

#include <stdio.h>
#include <string.h>

#include "ipc_internal.h"

void ipc_pipe_push(jvk_ipc_pipe_t* pipe, char c)
{
    pipe->buf[pipe->tail] = c;
    pipe->tail = (pipe->tail + 1) % pipe->capacity;
    pipe->count++;
}

char ipc_pipe_pop(jvk_ipc_pipe_t* pipe)
{
    char c = pipe->buf[pipe->head];
    pipe->head = (pipe->head + 1) % pipe->capacity;
    pipe->count--;
    return c;
}

static jvk_ipc_pipe_t* find_pipe(jvk_ipc_t* ipc, const char* name)
{
    if (ipc == NULL || name == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_IPC_MAX_PIPES; i++) {
        if (ipc->pipes[i].used && strcmp(ipc->pipes[i].name, name) == 0) {
            return &ipc->pipes[i];
        }
    }
    return NULL;
}

jvk_ipc_status_t ipc_pipe_create(jvk_ipc_t* ipc, const char* name, int owner,
                                 int capacity, unsigned long ticks)
{
    if (ipc == NULL || name == NULL || name[0] == '\0' ||
        strlen(name) >= JVK_IPC_NAME_LEN) {
        return JVK_IPC_ERR_ARG;
    }
    if (find_pipe(ipc, name) != NULL) {
        return JVK_IPC_ERR_EXIST;
    }
    if (capacity <= 0) {
        capacity = 256;
    }
    if (capacity > JVK_IPC_PIPE_LEN) {
        capacity = JVK_IPC_PIPE_LEN;
    }
    for (int i = 0; i < JVK_IPC_MAX_PIPES; i++) {
        jvk_ipc_pipe_t* p = &ipc->pipes[i];
        if (p->used) {
            continue;
        }
        memset(p, 0, sizeof(*p));
        p->used = 1;
        snprintf(p->name, sizeof(p->name), "%s", name);
        p->owner = owner;
        p->capacity = capacity;
        p->created_ticks = ticks;
        if (owner > 0) {
            p->readers[p->reader_count++] = owner;
            p->writers[p->writer_count++] = owner;
        }
        ipc->stats.pipes_created++;
        ipc_emit(ipc, "IPC_PIPE_CREATED name=%s owner=%d capacity=%d",
                 name, owner, capacity);
        return JVK_IPC_OK;
    }
    return JVK_IPC_ERR_FULL;
}

jvk_ipc_status_t ipc_pipe_open(jvk_ipc_t* ipc, const char* name, int pid,
                               const char* mode)
{
    jvk_ipc_pipe_t* p = find_pipe(ipc, name);
    if (p == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (mode == NULL || mode[0] == '\0') {
        mode = "rw";
    }
    int want_read = strchr(mode, 'r') != NULL;
    int want_write = strchr(mode, 'w') != NULL;
    if (!want_read && !want_write) {
        return JVK_IPC_ERR_ARG;
    }
    if (want_read && !ipc_array_contains(p->readers, p->reader_count, pid)) {
        if (p->reader_count >= JVK_IPC_MAX_WAITERS) {
            return JVK_IPC_ERR_FULL;
        }
        p->readers[p->reader_count++] = pid;
    }
    if (want_write && !ipc_array_contains(p->writers, p->writer_count, pid)) {
        if (p->writer_count >= JVK_IPC_MAX_WAITERS) {
            return JVK_IPC_ERR_FULL;
        }
        p->writers[p->writer_count++] = pid;
    }
    ipc_emit(ipc, "IPC_PIPE_OPEN name=%s pid=%d mode=%s", p->name, pid, mode);
    return JVK_IPC_OK;
}

/* Commit a blocked writer chunk into newly freed ring space. */
static int pipe_commit_writers(jvk_ipc_t* ipc, jvk_ipc_pipe_t* p)
{
    int committed = 0;
    for (int w = 0; w < p->write_wait_count;) {
        jvk_ipc_pipe_writer_t* wr = &p->write_waiters[w];
        while (wr->off < wr->len && p->count < p->capacity) {
            ipc_pipe_push(p, wr->data[wr->off++]);
            p->bytes_in++;
            ipc->stats.pipe_bytes++;
            committed++;
        }
        if (wr->off >= wr->len) {
            int pid = wr->pid;
            for (int j = w; j < p->write_wait_count - 1; j++) {
                p->write_waiters[j] = p->write_waiters[j + 1];
            }
            p->write_wait_count--;
            ipc_wake(ipc, pid);
            ipc_emit(ipc, "IPC_WRITE_WAKE name=%s pid=%d bytes=%d",
                     p->name, pid, wr->len);
        } else {
            w++;
        }
    }
    return committed;
}

jvk_ipc_status_t ipc_pipe_write(jvk_ipc_t* ipc, const char* name, int pid,
                                const char* data, int nonblock, int* written)
{
    jvk_ipc_pipe_t* p = find_pipe(ipc, name);
    if (written != NULL) {
        *written = 0;
    }
    if (p == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (data == NULL) {
        return JVK_IPC_ERR_ARG;
    }
    if (!ipc_array_contains(p->writers, p->writer_count, pid) &&
        !ipc_array_contains(p->readers, p->reader_count, pid)) {
        return JVK_IPC_ERR_PERM;
    }
    int len = (int)strlen(data);

    /* A blocked reader gets the next chunk directly. */
    if (p->read_wait_count > 0 && len > 0) {
        jvk_ipc_pipe_reader_t rd = p->read_waiters[0];
        for (int j = 0; j < p->read_wait_count - 1; j++) {
            p->read_waiters[j] = p->read_waiters[j + 1];
        }
        p->read_wait_count--;
        int take = len < rd.max ? len : rd.max;
        char tmp[JVK_IPC_PIPE_LEN + 1];
        if (take >= (int)sizeof(tmp)) {
            take = (int)sizeof(tmp) - 1;
        }
        memcpy(tmp, data, (size_t)take);
        tmp[take] = '\0';
        if (!ipc_delivery_push(ipc, rd.pid, tmp, take)) {
            return JVK_IPC_ERR_FULL;
        }
        p->writes++;
        p->bytes_in += (unsigned long)take;
        ipc->stats.pipe_writes++;
        ipc->stats.pipe_bytes += (unsigned long)take;
        ipc_wake(ipc, rd.pid);
        if (written != NULL) {
            *written = take;
        }
        ipc_emit(ipc, "IPC_PIPE_WRITE name=%s pid=%d bytes=%d wake=%d",
                 p->name, pid, take, rd.pid);

        /* Anything beyond the handoff still goes into the ring. */
        for (int i = take; i < len && p->count < p->capacity; i++) {
            ipc_pipe_push(p, data[i]);
            p->bytes_in++;
            ipc->stats.pipe_bytes++;
            if (written != NULL) {
                (*written)++;
            }
        }
        return JVK_IPC_OK;
    }

    int space = p->capacity - p->count;
    int take = len < space ? len : space;
    for (int i = 0; i < take; i++) {
        ipc_pipe_push(p, data[i]);
        p->bytes_in++;
        ipc->stats.pipe_bytes++;
    }
    if (written != NULL) {
        *written = take;
    }
    if (take > 0) {
        p->writes++;
        ipc->stats.pipe_writes++;
        ipc_emit(ipc, "IPC_PIPE_WRITE name=%s pid=%d bytes=%d",
                 p->name, pid, take);
    }
    if (take == len) {
        return JVK_IPC_OK;
    }
    if (nonblock) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (pid <= 0) {
        return JVK_IPC_ERR_AGAIN;
    }
    for (int i = 0; i < p->write_wait_count; i++) {
        if (p->write_waiters[i].pid == pid) {
            return JVK_IPC_ERR_BUSY;
        }
    }
    if (p->write_wait_count >= JVK_IPC_MAX_WAITERS) {
        return JVK_IPC_ERR_FULL;
    }
    jvk_ipc_pipe_writer_t* wr = &p->write_waiters[p->write_wait_count++];
    wr->pid = pid;
    wr->len = len - take;
    if (wr->len > JVK_IPC_PIPE_CHUNK) {
        wr->len = JVK_IPC_PIPE_CHUNK;
    }
    if (wr->len > 0) {
        memcpy(wr->data, data + take, (size_t)wr->len);
    }
    wr->off = 0;
    p->blocked_writes++;
    ipc->stats.blocked++;
    ipc_emit(ipc, "IPC_BLOCK name=%s pid=%d kind=pipe-write", p->name, pid);
    return JVK_IPC_BLOCKED;
}

jvk_ipc_status_t ipc_pipe_read(jvk_ipc_t* ipc, const char* name, int pid,
                               int max, int nonblock,
                               char* out, int cap, int* out_len)
{
    jvk_ipc_pipe_t* p = find_pipe(ipc, name);
    if (p == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (out == NULL || cap <= 0) {
        return JVK_IPC_ERR_ARG;
    }
    if (max <= 0 || max > cap - 1) {
        max = cap - 1;
    }
    if (!ipc_array_contains(p->readers, p->reader_count, pid) &&
        !ipc_array_contains(p->writers, p->writer_count, pid)) {
        return JVK_IPC_ERR_PERM;
    }

    jvk_ipc_delivery_t* slot = ipc_delivery_slot(ipc, pid, 0);
    if (slot != NULL) {
        int n = slot->len;
        if (n > max) {
            n = max;
        }
        if (n > 0) {
            memcpy(out, slot->data, (size_t)n);
        }
        out[n] = '\0';
        if (out_len != NULL) {
            *out_len = n;
        }
        slot->valid = 0;
        slot->len = 0;
        p->reads++;
        ipc->stats.pipe_reads++;
        ipc_emit(ipc, "IPC_PIPE_READ name=%s pid=%d bytes=%d", p->name, pid, n);
        return JVK_IPC_OK;
    }

    if (p->count > 0) {
        int n = 0;
        while (p->count > 0 && n < max) {
            out[n++] = ipc_pipe_pop(p);
        }
        out[n] = '\0';
        if (out_len != NULL) {
            *out_len = n;
        }
        p->reads++;
        p->bytes_out += (unsigned long)n;
        ipc->stats.pipe_reads++;
        ipc->stats.pipe_bytes += (unsigned long)n;
        pipe_commit_writers(ipc, p);
        ipc_emit(ipc, "IPC_PIPE_READ name=%s pid=%d bytes=%d", p->name, pid, n);
        return JVK_IPC_OK;
    }

    if (nonblock) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (pid <= 0) {
        return JVK_IPC_ERR_AGAIN;
    }
    for (int i = 0; i < p->read_wait_count; i++) {
        if (p->read_waiters[i].pid == pid) {
            return JVK_IPC_ERR_BUSY;
        }
    }
    if (p->read_wait_count >= JVK_IPC_MAX_WAITERS) {
        return JVK_IPC_ERR_FULL;
    }
    p->read_waiters[p->read_wait_count].pid = pid;
    p->read_waiters[p->read_wait_count].max = max;
    p->read_wait_count++;
    p->blocked_reads++;
    ipc->stats.blocked++;
    ipc_emit(ipc, "IPC_BLOCK name=%s pid=%d kind=pipe-read", p->name, pid);
    return JVK_IPC_BLOCKED;
}

jvk_ipc_status_t ipc_pipe_close(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_pipe_t* p = find_pipe(ipc, name);
    if (p == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    ipc_array_remove(p->readers, &p->reader_count, pid);
    ipc_array_remove(p->writers, &p->writer_count, pid);
    ipc_emit(ipc, "IPC_PIPE_CLOSE name=%s pid=%d", p->name, pid);
    return JVK_IPC_OK;
}

jvk_ipc_status_t ipc_pipe_destroy(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_pipe_t* p = find_pipe(ipc, name);
    if (p == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (pid != 0 && p->owner != pid && p->owner != 0) {
        return JVK_IPC_ERR_PERM;
    }
    for (int i = 0; i < p->read_wait_count; i++) {
        ipc_wake(ipc, p->read_waiters[i].pid);
    }
    for (int i = 0; i < p->write_wait_count; i++) {
        ipc_wake(ipc, p->write_waiters[i].pid);
    }
    ipc_emit(ipc, "IPC_PIPE_DESTROYED name=%s pid=%d", p->name, pid);
    memset(p, 0, sizeof(*p));
    return JVK_IPC_OK;
}
