/*
 * kernel/shell/shell_format.c
 *
 * JARVIS OS — output formatting for shell results (M8).
 * All values come from the kernel reply; nothing is invented here.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "shell_internal.h"

void sh_out(sh_ctx_t* ctx, const char* fmt, ...)
{
    if (ctx == NULL || ctx->out == NULL || ctx->len >= ctx->cap) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    int written = vsnprintf(ctx->out + ctx->len, ctx->cap - ctx->len, fmt, ap);
    va_end(ap);
    if (written < 0) {
        return;
    }
    if ((size_t)written >= ctx->cap - ctx->len) {
        ctx->len = ctx->cap - 1;
    } else {
        ctx->len += (size_t)written;
    }
}

static const char* jstr(const cJSON* obj, const char* key, const char* def)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(item) ? item->valuestring : def;
}

static int jint(const cJSON* obj, const char* key, int def)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? (int)item->valuedouble : def;
}

static int jbool(const cJSON* obj, const char* key, int def)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsBool(item) ? (cJSON_IsTrue(item) ? 1 : 0) : def;
}

void sh_fmt_processes(sh_ctx_t* ctx, const cJSON* result)
{
    const cJSON* list = cJSON_GetObjectItemCaseSensitive(result, "processes");
    sh_out(ctx, "PID  NAME                 STATE      PRI  BURST  CPU  MEM\n");
    int count = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
    for (int i = 0; i < count; i++) {
        const cJSON* p = cJSON_GetArrayItem(list, i);
        sh_out(ctx, "%-4d %-20s %-10s %-4d %-6d %-4d %d/%d\n",
               jint(p, "pid", 0), jstr(p, "name", "?"),
               jstr(p, "state", "?"), jint(p, "priority", 0),
               jint(p, "burst_time", 0), jint(p, "cpu_used", 0),
               jint(p, "mem_pages", 0), jint(p, "mem_budget_pages", 0));
    }
    if (count == 0) {
        sh_out(ctx, "(no processes)\n");
    }
    const cJSON* q = cJSON_GetObjectItemCaseSensitive(result, "queues");
    for (const char* key = "ready"; key != NULL; key = NULL) {
        const cJSON* arr = cJSON_GetObjectItemCaseSensitive(q, key);
        sh_out(ctx, "ready=[");
        for (int i = 0; i < cJSON_GetArraySize(arr); i++) {
            sh_out(ctx, "%s%d", i > 0 ? "," : "",
                   (int)cJSON_GetArrayItem(arr, i)->valuedouble);
        }
        sh_out(ctx, "]\n");
    }
}

void sh_fmt_memory(sh_ctx_t* ctx, const cJSON* result)
{
    const cJSON* mem = cJSON_GetObjectItemCaseSensitive(result, "memory");
    if (!cJSON_IsObject(mem)) {
        sh_out(ctx, "memory: no data\n");
        return;
    }
    const cJSON* stats = cJSON_GetObjectItemCaseSensitive(mem, "stats");
    const cJSON* cfg = cJSON_GetObjectItemCaseSensitive(mem, "config");
    sh_out(ctx, "frames %d used / %d free   pages mapped=%d\n",
           jint(stats, "frames_used", 0), jint(stats, "frames_free", 0),
           jint(stats, "pages_mapped", 0));
    sh_out(ctx, "allocs=%d frees=%d faults=%d segv=%d swap in/out=%d/%d\n",
           jint(stats, "allocs", 0), jint(stats, "frees", 0),
           jint(stats, "page_faults", 0), jint(stats, "segfaults", 0),
           jint(stats, "swap_ins", 0), jint(stats, "swap_outs", 0));
    sh_out(ctx, "allocator=%s replacement=%s swap=%s\n",
           jstr(cfg, "allocator", "?"), jstr(cfg, "replacement", "?"),
           jbool(cfg, "swap_enabled", 0) ? "on" : "off");
    const cJSON* tables = cJSON_GetObjectItemCaseSensitive(mem, "page_tables");
    sh_out(ctx, "page tables:\n");
    for (int i = 0; i < cJSON_GetArraySize(tables); i++) {
        const cJSON* t = cJSON_GetArrayItem(tables, i);
        sh_out(ctx, "  pid %-4d used=%d budget=%d\n", jint(t, "pid", 0),
               jint(t, "used", 0), jint(t, "budget", 0));
    }
}

void sh_fmt_entries(sh_ctx_t* ctx, const cJSON* result)
{
    const cJSON* list = cJSON_GetObjectItemCaseSensitive(result, "entries");
    int count = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
    for (int i = 0; i < count; i++) {
        const cJSON* e = cJSON_GetArrayItem(list, i);
        sh_out(ctx, "%-5s %-24s size=%-6d mode=%s owner=%d\n",
               jstr(e, "type", "?"), jstr(e, "name", "?"),
               jint(e, "size", 0), jstr(e, "mode", "?"),
               jint(e, "owner", 0));
    }
    if (count == 0) {
        sh_out(ctx, "(empty directory)\n");
    }
}

void sh_fmt_devices(sh_ctx_t* ctx, const cJSON* result)
{
    const cJSON* devices = cJSON_GetObjectItemCaseSensitive(result, "devices");
    const cJSON* list = cJSON_GetObjectItemCaseSensitive(devices, "list");
    int count = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
    sh_out(ctx, "DEVICE       DRIVER       STATE     IRQ       PWR  IRQS  R/W\n");
    for (int i = 0; i < count; i++) {
        const cJSON* d = cJSON_GetArrayItem(list, i);
        sh_out(ctx, "%-12s %-12s %-9s %-9s %-4s %-5d %d/%d%s\n",
               jstr(d, "name", "?"), jstr(d, "driver", "?"),
               jstr(d, "state", "?"), jstr(d, "irq", "-"),
               jbool(d, "power", 0) ? "on" : "off",
               jint(d, "irq_count", 0), jint(d, "reads", 0),
               jint(d, "writes", 0),
               jint(d, "errors", 0) > 0 ? "  ERRORS" : "");
    }
}

void sh_fmt_ipc(sh_ctx_t* ctx, const cJSON* result)
{
    const cJSON* ipc = cJSON_GetObjectItemCaseSensitive(result, "ipc");
    const cJSON* stats = cJSON_GetObjectItemCaseSensitive(ipc, "stats");
    sh_out(ctx, "msgs sent=%d recv=%d | shm writes=%d reads=%d | "
                "pipe reads=%d writes=%d | deadlocks=%d\n",
           jint(stats, "messages_sent", 0), jint(stats, "messages_received", 0),
           jint(stats, "shm_writes", 0), jint(stats, "shm_reads", 0),
           jint(stats, "pipe_reads", 0), jint(stats, "pipe_writes", 0),
           jint(stats, "deadlocks", 0));
    const char* names[] = {"queues", "segments", "pipes", "mutexes", "semaphores"};
    const char* labels[] = {"QUEUE", "SHM", "PIPE", "MUTEX", "SEM"};
    for (int k = 0; k < 5; k++) {
        const cJSON* arr = cJSON_GetObjectItemCaseSensitive(ipc, names[k]);
        for (int i = 0; i < cJSON_GetArraySize(arr); i++) {
            const cJSON* o = cJSON_GetArrayItem(arr, i);
            sh_out(ctx, "  %-5s %-16s owner=%d", labels[k],
                   jstr(o, "name", "?"), jint(o, "owner", 0));
            if (k == 0) {
                sh_out(ctx, " count=%d/%d", jint(o, "count", 0),
                       jint(o, "capacity", 0));
            } else if (k == 1) {
                sh_out(ctx, " len=%d/%d", jint(o, "length", 0),
                       jint(o, "size", 0));
            } else if (k == 2) {
                sh_out(ctx, " count=%d/%d", jint(o, "count", 0),
                       jint(o, "capacity", 0));
            } else if (k == 3) {
                sh_out(ctx, " holder=%d rec=%d", jint(o, "owner", 0),
                       jint(o, "recursion", 0));
            } else {
                sh_out(ctx, " value=%d/%d", jint(o, "value", 0),
                       jint(o, "max", 0));
            }
            sh_out(ctx, "\n");
        }
    }
    const cJSON* graph = cJSON_GetObjectItemCaseSensitive(ipc, "wait_graph");
    const cJSON* cycles = cJSON_GetObjectItemCaseSensitive(graph, "cycles");
    if (jbool(graph, "active", 0)) {
        sh_out(ctx, "DEADLOCK ACTIVE cycles=%d\n", cJSON_GetArraySize(cycles));
    }
}
