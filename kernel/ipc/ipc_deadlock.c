/*
 * kernel/ipc/ipc_deadlock.c
 *
 * JARVIS OS — wait-for graph and deadlock detection (M7).
 *
 * Every blocked mutex owner and every semaphore permit holder contributes
 * an edge "waiter -> holder". A cycle in that graph is a deadlock: the
 * processes in the cycle can only progress if one of them releases a
 * resource they are themselves waiting for.
 *
 * The detector is deterministic, deduplicates cycles and only bumps the
 * deadlock counter when the cycle set changes, so a stuck system does not
 * flood the kernel log.
 */

#include <string.h>

#include "ipc_internal.h"

#define IPC_MAX_NODES 16

static int edge_exists(const jvk_ipc_edge_t* edges, int n,
                       int pid, int target, const char* resource)
{
    for (int i = 0; i < n; i++) {
        if (edges[i].pid == pid && edges[i].target == target &&
            strcmp(edges[i].resource, resource) == 0) {
            return 1;
        }
    }
    return 0;
}

int ipc_wait_edges(const jvk_ipc_t* ipc, jvk_ipc_edge_t* edges, int cap)
{
    if (ipc == NULL || edges == NULL || cap <= 0) {
        return 0;
    }
    int n = 0;

    for (int i = 0; i < JVK_IPC_MAX_MUTEXES && n < cap; i++) {
        const jvk_ipc_mutex_t* m = &ipc->mutexes[i];
        if (!m->used || m->owner <= 0) {
            continue;
        }
        for (int w = 0; w < m->waiter_count && n < cap; w++) {
            if (m->waiters[w] == m->owner) {
                continue;
            }
            if (edge_exists(edges, n, m->waiters[w], m->owner, m->name)) {
                continue;
            }
            edges[n].pid = m->waiters[w];
            edges[n].target = m->owner;
            edges[n].kind = "mutex";
            edges[n].resource = m->name;
            n++;
        }
    }

    for (int i = 0; i < JVK_IPC_MAX_SEMS && n < cap; i++) {
        const jvk_ipc_sem_t* s = &ipc->sems[i];
        if (!s->used) {
            continue;
        }
        for (int w = 0; w < s->waiter_count && n < cap; w++) {
            for (int h = 0; h < s->holder_count && n < cap; h++) {
                if (s->waiters[w] == s->holders[h]) {
                    continue;
                }
                if (edge_exists(edges, n, s->waiters[w], s->holders[h], s->name)) {
                    continue;
                }
                edges[n].pid = s->waiters[w];
                edges[n].target = s->holders[h];
                edges[n].kind = "semaphore";
                edges[n].resource = s->name;
                n++;
            }
        }
    }
    return n;
}

static int node_index(int* nodes, int* node_count, int pid)
{
    for (int i = 0; i < *node_count; i++) {
        if (nodes[i] == pid) {
            return i;
        }
    }
    if (*node_count >= IPC_MAX_NODES) {
        return -1;
    }
    nodes[*node_count] = pid;
    return (*node_count)++;
}

static int cycle_exists(const jvk_ipc_cycle_t* cycles, int count, int* pids,
                        int len)
{
    int sorted[JVK_IPC_MAX_PROCS];
    if (len > JVK_IPC_MAX_PROCS) {
        len = JVK_IPC_MAX_PROCS;
    }
    for (int i = 0; i < len; i++) {
        sorted[i] = pids[i];
    }
    for (int i = 0; i < len; i++) {
        for (int j = i + 1; j < len; j++) {
            if (sorted[j] < sorted[i]) {
                int t = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = t;
            }
        }
    }
    for (int c = 0; c < count; c++) {
        if (cycles[c].count != len) {
            continue;
        }
        int same = 1;
        for (int i = 0; i < len; i++) {
            if (cycles[c].pids[i] != sorted[i]) {
                same = 0;
                break;
            }
        }
        if (same) {
            return 1;
        }
    }
    return 0;
}

static void dfs_cycles(const jvk_ipc_edge_t* edges, int edge_count,
                       int* nodes, int node_count, int u,
                       int* color, int* stack, int* stack_len,
                       jvk_ipc_cycle_t* cycles, int* cycle_count, int cap)
{
    color[u] = 1;
    stack[(*stack_len)++] = nodes[u];

    for (int e = 0; e < edge_count; e++) {
        if (edges[e].pid != nodes[u]) {
            continue;
        }
        int v = node_index(nodes, &node_count, edges[e].target);
        if (v < 0) {
            continue;
        }
        if (color[v] == 0) {
            dfs_cycles(edges, edge_count, nodes, node_count, v, color,
                       stack, stack_len, cycles, cycle_count, cap);
        } else if (color[v] == 1) {
            int start = -1;
            for (int i = 0; i < *stack_len; i++) {
                if (stack[i] == nodes[v]) {
                    start = i;
                    break;
                }
            }
            if (start >= 0 && *cycle_count < cap) {
                int len = *stack_len - start;
                if (!cycle_exists(cycles, *cycle_count, stack + start, len)) {
                    jvk_ipc_cycle_t* cyc = &cycles[*cycle_count];
                    cyc->count = len > JVK_IPC_MAX_PROCS
                                     ? JVK_IPC_MAX_PROCS : len;
                    for (int i = 0; i < cyc->count; i++) {
                        cyc->pids[i] = stack[start + i];
                    }
                    (*cycle_count)++;
                }
            }
        }
    }

    color[u] = 2;
    (*stack_len)--;
}

int ipc_detect_deadlock(jvk_ipc_t* ipc, jvk_ipc_cycle_t* cycles, int cap)
{
    if (ipc == NULL || cycles == NULL || cap <= 0) {
        return 0;
    }
    jvk_ipc_edge_t edges[JVK_IPC_MAX_EDGES];
    int edge_count = ipc_wait_edges(ipc, edges, JVK_IPC_MAX_EDGES);

    int nodes[IPC_MAX_NODES];
    int node_count = 0;
    for (int e = 0; e < edge_count; e++) {
        int a = node_index(nodes, &node_count, edges[e].pid);
        int b = node_index(nodes, &node_count, edges[e].target);
        (void)a;
        (void)b;
    }

    int color[IPC_MAX_NODES] = {0};
    int stack[IPC_MAX_NODES];
    int stack_len = 0;
    int cycle_count = 0;
    for (int i = 0; i < node_count && cycle_count < cap; i++) {
        if (color[i] == 0) {
            dfs_cycles(edges, edge_count, nodes, node_count, i, color,
                       stack, &stack_len, cycles, &cycle_count, cap);
        }
    }

    if (cycle_count > 0) {
        unsigned long sig = 2166136261u;
        for (int c = 0; c < cycle_count; c++) {
            for (int i = 0; i < cycles[c].count; i++) {
                sig ^= (unsigned long)cycles[c].pids[i];
                sig *= 16777619u;
            }
        }
        if (sig != ipc->deadlock_signature) {
            ipc->deadlock_signature = sig;
            ipc->stats.deadlocks++;
            ipc_emit(ipc, "IPC_DEADLOCK cycles=%d", cycle_count);
        }
        ipc->deadlock_active = 1;
    } else {
        ipc->deadlock_signature = 0;
        ipc->deadlock_active = 0;
    }
    return cycle_count;
}
