/*
 * kernel/ipc/ipc_queue.c
 *
 * JARVIS OS — bounded message queues (M7).
 *
 * Send blocks when the queue is full; receive blocks when it is empty.
 * A blocked sender's message is held in the wait record and committed
 * as soon as a slot frees. A blocked receiver gets the next message
 * handed straight into its delivery slot, so no message is lost across
 * a wakeup.
 */

#include <stdio.h>
#include <string.h>

#include "ipc_internal.h"

static jvk_ipc_queue_t* find_queue(jvk_ipc_t* ipc, const char* name)
{
    if (ipc == NULL || name == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_IPC_MAX_QUEUES; i++) {
        if (ipc->queues[i].used && strcmp(ipc->queues[i].name, name) == 0) {
            return &ipc->queues[i];
        }
    }
    return NULL;
}

jvk_ipc_status_t ipc_queue_create(jvk_ipc_t* ipc, const char* name, int owner,
                                  int capacity, unsigned long ticks)
{
    if (ipc == NULL || name == NULL || name[0] == '\0' ||
        strlen(name) >= JVK_IPC_NAME_LEN) {
        return JVK_IPC_ERR_ARG;
    }
    if (find_queue(ipc, name) != NULL) {
        return JVK_IPC_ERR_EXIST;
    }
    if (capacity <= 0) {
        capacity = 8;
    }
    if (capacity > JVK_IPC_QUEUE_CAP) {
        capacity = JVK_IPC_QUEUE_CAP;
    }
    for (int i = 0; i < JVK_IPC_MAX_QUEUES; i++) {
        jvk_ipc_queue_t* q = &ipc->queues[i];
        if (q->used) {
            continue;
        }
        memset(q, 0, sizeof(*q));
        q->used = 1;
        snprintf(q->name, sizeof(q->name), "%s", name);
        q->owner = owner;
        q->capacity = capacity;
        q->created_ticks = ticks;
        ipc->stats.queues_created++;
        ipc_emit(ipc, "IPC_QUEUE_CREATED name=%s owner=%d capacity=%d",
                 name, owner, capacity);
        return JVK_IPC_OK;
    }
    return JVK_IPC_ERR_FULL;
}

static void queue_push(jvk_ipc_queue_t* q, const char* data)
{
    snprintf(q->msgs[q->tail], JVK_IPC_MSG_LEN, "%s", data);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
}

static void queue_pop(jvk_ipc_queue_t* q, char* out, int cap, int* len)
{
    snprintf(out, (size_t)cap, "%s", q->msgs[q->head]);
    if (len != NULL) {
        *len = (int)strlen(q->msgs[q->head]);
    }
    q->head = (q->head + 1) % q->capacity;
    q->count--;
}

jvk_ipc_status_t ipc_queue_send(jvk_ipc_t* ipc, const char* name, int pid,
                                const char* data, int nonblock)
{
    jvk_ipc_queue_t* q = find_queue(ipc, name);
    if (q == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (data == NULL || strlen(data) >= JVK_IPC_MSG_LEN) {
        return JVK_IPC_ERR_ARG;
    }

    /* A waiting receiver takes the message directly. */
    if (q->receiver_count > 0) {
        int receiver = q->receivers[0];
        ipc_array_remove(q->receivers, &q->receiver_count, receiver);
        if (!ipc_delivery_push(ipc, receiver, data, (int)strlen(data))) {
            return JVK_IPC_ERR_FULL;
        }
        ipc_wake(ipc, receiver);
        ipc->stats.messages_sent++;
        ipc_emit(ipc, "IPC_RECV_WAKE queue=%s pid=%d bytes=%d",
                 q->name, receiver, (int)strlen(data));
        return JVK_IPC_OK;
    }

    if (q->count < q->capacity) {
        queue_push(q, data);
        q->sent++;
        ipc->stats.messages_sent++;
        ipc_emit(ipc, "IPC_MSG_SENT queue=%s pid=%d bytes=%d",
                 q->name, pid, (int)strlen(data));
        return JVK_IPC_OK;
    }
    if (nonblock) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (pid <= 0) {
        return JVK_IPC_ERR_AGAIN; /* system callers cannot wait */
    }
    for (int i = 0; i < q->sender_count; i++) {
        if (q->senders[i].pid == pid) {
            return JVK_IPC_ERR_BUSY;
        }
    }
    if (q->sender_count >= JVK_IPC_MAX_WAITERS) {
        return JVK_IPC_ERR_FULL;
    }
    q->senders[q->sender_count].pid = pid;
    snprintf(q->senders[q->sender_count].data, JVK_IPC_MSG_LEN, "%s", data);
    q->sender_count++;
    q->blocked_sends++;
    ipc->stats.blocked++;
    ipc_emit(ipc, "IPC_BLOCK queue=%s pid=%d kind=send", q->name, pid);
    return JVK_IPC_BLOCKED;
}

jvk_ipc_status_t ipc_take_delivery(jvk_ipc_t* ipc, int pid,
                                   char* out, int cap, int* out_len)
{
    jvk_ipc_delivery_t* slot = ipc_delivery_slot(ipc, pid, 0);
    if (slot == NULL) {
        return JVK_IPC_ERR_EMPTY;
    }
    int n = slot->len;
    if (n >= cap) {
        n = cap - 1;
    }
    if (n > 0) {
        memcpy(out, slot->data, (size_t)n);
    }
    if (cap > 0) {
        out[n] = '\0';
    }
    if (out_len != NULL) {
        *out_len = n;
    }
    slot->valid = 0;
    slot->len = 0;
    ipc->stats.messages_received++;
    return JVK_IPC_OK;
}

jvk_ipc_status_t ipc_queue_recv(jvk_ipc_t* ipc, const char* name, int pid,
                                int nonblock, char* out, int cap, int* out_len)
{
    jvk_ipc_queue_t* q = find_queue(ipc, name);
    if (q == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (out == NULL || cap <= 0) {
        return JVK_IPC_ERR_ARG;
    }

    if (ipc_delivery_slot(ipc, pid, 0) != NULL) {
        jvk_ipc_status_t st = ipc_take_delivery(ipc, pid, out, cap, out_len);
        if (st == JVK_IPC_OK) {
            ipc_emit(ipc, "IPC_MSG_RECV queue=%s pid=%d bytes=%d",
                     q->name, pid, out_len != NULL ? *out_len : 0);
            return st;
        }
    }

    if (q->count > 0) {
        queue_pop(q, out, cap, out_len);
        q->received++;
        ipc->stats.messages_received++;
        ipc_emit(ipc, "IPC_MSG_RECV queue=%s pid=%d bytes=%d",
                 q->name, pid, out_len != NULL ? *out_len : 0);

        /* A freed slot commits the oldest blocked sender immediately. */
        if (q->sender_count > 0) {
            jvk_ipc_msg_waiter_t sender = q->senders[0];
            for (int j = 0; j < q->sender_count - 1; j++) {
                q->senders[j] = q->senders[j + 1];
            }
            q->sender_count--;
            queue_push(q, sender.data);
            q->sent++;
            ipc->stats.messages_sent++;
            ipc_wake(ipc, sender.pid);
            ipc_emit(ipc, "IPC_SEND_WAKE queue=%s pid=%d bytes=%d",
                     q->name, sender.pid, (int)strlen(sender.data));
        }
        return JVK_IPC_OK;
    }
    if (nonblock) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (pid <= 0) {
        return JVK_IPC_ERR_AGAIN;
    }
    if (ipc_array_contains(q->receivers, q->receiver_count, pid)) {
        return JVK_IPC_ERR_BUSY;
    }
    if (q->receiver_count >= JVK_IPC_MAX_WAITERS) {
        return JVK_IPC_ERR_FULL;
    }
    q->receivers[q->receiver_count++] = pid;
    q->blocked_recvs++;
    ipc->stats.blocked++;
    ipc_emit(ipc, "IPC_BLOCK queue=%s pid=%d kind=recv", q->name, pid);
    return JVK_IPC_BLOCKED;
}

jvk_ipc_status_t ipc_queue_destroy(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_queue_t* q = find_queue(ipc, name);
    if (q == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (pid != 0 && q->owner != pid && q->owner != 0) {
        return JVK_IPC_ERR_PERM;
    }
    ipc->stats.messages_dropped += (unsigned long)q->sender_count;
    for (int i = 0; i < q->receiver_count; i++) {
        ipc_wake(ipc, q->receivers[i]);
    }
    for (int i = 0; i < q->sender_count; i++) {
        ipc_wake(ipc, q->senders[i].pid);
    }
    ipc_emit(ipc, "IPC_QUEUE_DESTROYED name=%s pid=%d", q->name, pid);
    memset(q, 0, sizeof(*q));
    return JVK_IPC_OK;
}
