/*
 * kernel/core/kernel_ipc_snapshot.c
 *
 * JARVIS OS — "ipc" snapshot object (M7).
 *
 * Everything the IPC Lab renders comes from here: queues, segments,
 * pipes, mutexes, semaphores, pending deliveries and the current
 * wait-for graph with its detected cycles.
 */

#include <string.h>

#include "kernel_ipc.h"
#include "../ipc/ipc_internal.h"

static void add_stats(const jvk_ipc_t* ipc, cJSON* root)
{
    cJSON* s = cJSON_CreateObject();
    cJSON_AddNumberToObject(s, "queues_created", (double)ipc->stats.queues_created);
    cJSON_AddNumberToObject(s, "messages_sent", (double)ipc->stats.messages_sent);
    cJSON_AddNumberToObject(s, "messages_received", (double)ipc->stats.messages_received);
    cJSON_AddNumberToObject(s, "messages_dropped", (double)ipc->stats.messages_dropped);
    cJSON_AddNumberToObject(s, "segments_created", (double)ipc->stats.segments_created);
    cJSON_AddNumberToObject(s, "shm_maps", (double)ipc->stats.shm_maps);
    cJSON_AddNumberToObject(s, "shm_writes", (double)ipc->stats.shm_writes);
    cJSON_AddNumberToObject(s, "shm_reads", (double)ipc->stats.shm_reads);
    cJSON_AddNumberToObject(s, "pipes_created", (double)ipc->stats.pipes_created);
    cJSON_AddNumberToObject(s, "pipe_reads", (double)ipc->stats.pipe_reads);
    cJSON_AddNumberToObject(s, "pipe_writes", (double)ipc->stats.pipe_writes);
    cJSON_AddNumberToObject(s, "pipe_bytes", (double)ipc->stats.pipe_bytes);
    cJSON_AddNumberToObject(s, "mutex_locks", (double)ipc->stats.mutex_locks);
    cJSON_AddNumberToObject(s, "mutex_unlocks", (double)ipc->stats.mutex_unlocks);
    cJSON_AddNumberToObject(s, "mutex_contentions", (double)ipc->stats.mutex_contentions);
    cJSON_AddNumberToObject(s, "sem_waits", (double)ipc->stats.sem_waits);
    cJSON_AddNumberToObject(s, "sem_posts", (double)ipc->stats.sem_posts);
    cJSON_AddNumberToObject(s, "sem_contentions", (double)ipc->stats.sem_contentions);
    cJSON_AddNumberToObject(s, "blocked", (double)ipc->stats.blocked);
    cJSON_AddNumberToObject(s, "woken", (double)ipc->stats.woken);
    cJSON_AddNumberToObject(s, "deadlocks", (double)ipc->stats.deadlocks);
    cJSON_AddNumberToObject(s, "recoveries", (double)ipc->stats.recoveries);
    cJSON_AddItemToObject(root, "stats", s);
}

static void add_int_array(cJSON* parent, const char* key,
                          const int* values, int count)
{
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(values[i]));
    }
    cJSON_AddItemToObject(parent, key, arr);
}

static void add_queues(const jvk_ipc_t* ipc, cJSON* root)
{
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < JVK_IPC_MAX_QUEUES; i++) {
        const jvk_ipc_queue_t* q = &ipc->queues[i];
        if (!q->used) {
            continue;
        }
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", q->name);
        cJSON_AddNumberToObject(o, "owner", q->owner);
        cJSON_AddNumberToObject(o, "capacity", q->capacity);
        cJSON_AddNumberToObject(o, "count", q->count);
        cJSON_AddNumberToObject(o, "sent", (double)q->sent);
        cJSON_AddNumberToObject(o, "received", (double)q->received);
        cJSON_AddNumberToObject(o, "blocked_sends", (double)q->blocked_sends);
        cJSON_AddNumberToObject(o, "blocked_recvs", (double)q->blocked_recvs);
        cJSON* senders = cJSON_CreateArray();
        for (int w = 0; w < q->sender_count; w++) {
            cJSON_AddItemToArray(senders, cJSON_CreateNumber(q->senders[w].pid));
        }
        cJSON_AddItemToObject(o, "senders", senders);
        add_int_array(o, "receivers", q->receivers, q->receiver_count);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(root, "queues", arr);
}

static void add_segments(const jvk_ipc_t* ipc, cJSON* root)
{
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < JVK_IPC_MAX_SEGMENTS; i++) {
        const jvk_ipc_segment_t* seg = &ipc->segments[i];
        if (!seg->used) {
            continue;
        }
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", seg->name);
        cJSON_AddNumberToObject(o, "owner", seg->owner);
        cJSON_AddNumberToObject(o, "size", seg->size);
        cJSON_AddNumberToObject(o, "length", seg->length);
        cJSON_AddStringToObject(o, "data", seg->data);
        cJSON_AddNumberToObject(o, "maps", (double)seg->maps);
        cJSON_AddNumberToObject(o, "writes", (double)seg->writes);
        cJSON_AddNumberToObject(o, "reads", (double)seg->reads);
        add_int_array(o, "attached", seg->attached, seg->attach_count);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(root, "segments", arr);
}

static void add_pipes(const jvk_ipc_t* ipc, cJSON* root)
{
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < JVK_IPC_MAX_PIPES; i++) {
        const jvk_ipc_pipe_t* p = &ipc->pipes[i];
        if (!p->used) {
            continue;
        }
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", p->name);
        cJSON_AddNumberToObject(o, "owner", p->owner);
        cJSON_AddNumberToObject(o, "capacity", p->capacity);
        cJSON_AddNumberToObject(o, "count", p->count);
        cJSON_AddNumberToObject(o, "bytes_in", (double)p->bytes_in);
        cJSON_AddNumberToObject(o, "bytes_out", (double)p->bytes_out);
        cJSON_AddNumberToObject(o, "reads", (double)p->reads);
        cJSON_AddNumberToObject(o, "writes", (double)p->writes);
        add_int_array(o, "readers", p->readers, p->reader_count);
        add_int_array(o, "writers", p->writers, p->writer_count);
        cJSON* rw = cJSON_CreateArray();
        for (int w = 0; w < p->read_wait_count; w++) {
            cJSON_AddItemToArray(rw, cJSON_CreateNumber(p->read_waiters[w].pid));
        }
        cJSON_AddItemToObject(o, "read_waiters", rw);
        cJSON* ww = cJSON_CreateArray();
        for (int w = 0; w < p->write_wait_count; w++) {
            cJSON_AddItemToArray(ww, cJSON_CreateNumber(p->write_waiters[w].pid));
        }
        cJSON_AddItemToObject(o, "write_waiters", ww);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(root, "pipes", arr);
}

static void add_sync(const jvk_ipc_t* ipc, cJSON* root)
{
    cJSON* mutexes = cJSON_CreateArray();
    for (int i = 0; i < JVK_IPC_MAX_MUTEXES; i++) {
        const jvk_ipc_mutex_t* m = &ipc->mutexes[i];
        if (!m->used) {
            continue;
        }
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", m->name);
        cJSON_AddNumberToObject(o, "owner", m->owner);
        cJSON_AddNumberToObject(o, "recursion", m->recursion);
        cJSON_AddNumberToObject(o, "locks", (double)m->locks);
        cJSON_AddNumberToObject(o, "unlocks", (double)m->unlocks);
        cJSON_AddNumberToObject(o, "contentions", (double)m->contentions);
        add_int_array(o, "waiters", m->waiters, m->waiter_count);
        cJSON_AddItemToArray(mutexes, o);
    }
    cJSON_AddItemToObject(root, "mutexes", mutexes);

    cJSON* sems = cJSON_CreateArray();
    for (int i = 0; i < JVK_IPC_MAX_SEMS; i++) {
        const jvk_ipc_sem_t* s = &ipc->sems[i];
        if (!s->used) {
            continue;
        }
        cJSON* o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", s->name);
        cJSON_AddNumberToObject(o, "value", s->value);
        cJSON_AddNumberToObject(o, "max", s->max);
        cJSON_AddNumberToObject(o, "waits", (double)s->waits);
        cJSON_AddNumberToObject(o, "posts", (double)s->posts);
        cJSON_AddNumberToObject(o, "contentions", (double)s->contentions);
        add_int_array(o, "holders", s->holders, s->holder_count);
        add_int_array(o, "waiters", s->waiters, s->waiter_count);
        cJSON_AddItemToArray(sems, o);
    }
    cJSON_AddItemToObject(root, "semaphores", sems);
}

static void add_deliveries(const jvk_ipc_t* ipc, cJSON* root)
{
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < JVK_IPC_MAX_PROCS; i++) {
        const jvk_ipc_delivery_t* d = &ipc->deliveries[i];
        if (!d->valid) {
            continue;
        }
        cJSON* o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "pid", d->pid);
        cJSON_AddNumberToObject(o, "bytes", d->len);
        cJSON_AddStringToObject(o, "data", d->data);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(root, "deliveries", arr);
}

static void add_wait_graph(jvk_ipc_t* ipc, cJSON* root)
{
    jvk_ipc_edge_t edges[JVK_IPC_MAX_EDGES];
    int n = ipc_wait_edges(ipc, edges, JVK_IPC_MAX_EDGES);
    jvk_ipc_cycle_t cycles[JVK_IPC_MAX_CYCLES];
    int cycle_count = ipc_detect_deadlock(ipc, cycles, JVK_IPC_MAX_CYCLES);

    cJSON* graph = cJSON_CreateObject();
    cJSON* edge_arr = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON* o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "pid", edges[i].pid);
        cJSON_AddNumberToObject(o, "target", edges[i].target);
        cJSON_AddStringToObject(o, "kind", edges[i].kind);
        cJSON_AddStringToObject(o, "resource", edges[i].resource);
        cJSON_AddItemToArray(edge_arr, o);
    }
    cJSON_AddItemToObject(graph, "edges", edge_arr);

    cJSON* cycle_arr = cJSON_CreateArray();
    for (int c = 0; c < cycle_count; c++) {
        cJSON* one = cJSON_CreateArray();
        for (int i = 0; i < cycles[c].count; i++) {
            cJSON_AddItemToArray(one, cJSON_CreateNumber(cycles[c].pids[i]));
        }
        cJSON_AddItemToArray(cycle_arr, one);
    }
    cJSON_AddItemToObject(graph, "cycles", cycle_arr);
    cJSON_AddBoolToObject(graph, "active", ipc->deadlock_active != 0);
    cJSON_AddItemToObject(root, "wait_graph", graph);
}

void kipc_snapshot(const jvk_ipc_t* ipc, cJSON* root)
{
    if (ipc == NULL || root == NULL) {
        return;
    }
    cJSON* out = cJSON_CreateObject();
    cJSON* cfg = cJSON_CreateObject();
    cJSON_AddNumberToObject(cfg, "queue_capacity", ipc->cfg_queue_capacity);
    cJSON_AddNumberToObject(cfg, "pipe_capacity", ipc->cfg_pipe_capacity);
    cJSON_AddNumberToObject(cfg, "shm_bytes", ipc->cfg_shm_bytes);
    cJSON_AddItemToObject(out, "config", cfg);

    add_stats(ipc, out);
    add_queues(ipc, out);
    add_segments(ipc, out);
    add_pipes(ipc, out);
    add_sync(ipc, out);
    add_deliveries(ipc, out);
    add_wait_graph((jvk_ipc_t*)ipc, out);

    cJSON_AddItemToObject(root, "ipc", out);
}
