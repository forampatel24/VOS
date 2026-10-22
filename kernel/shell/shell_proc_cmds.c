/*
 * kernel/shell/shell_proc_cmds.c
 *
 * JARVIS OS — process, memory, scheduler, device and IPC commands (M8).
 */

#include <stdio.h>
#include <string.h>

#include "shell_internal.h"

static int cmd_ps(sh_ctx_t* ctx)
{
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "list_processes");
    cJSON* result = sh_call(ctx, req);
    sh_fmt_processes(ctx, result);
    cJSON_Delete(result);
    return 1;
}

static int cmd_pid_action(sh_ctx_t* ctx, int argc, char** argv,
                          const char* action, const char* verb)
{
    if (argc < 2) {
        sh_out(ctx, "usage: %s <pid|name>\n", argv[0]);
        return 1;
    }
    int pid = sh_pid_arg(ctx, argv[1]);
    if (pid <= 0) {
        return 1;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", action);
    cJSON_AddNumberToObject(req, "pid", pid);
    cJSON* result = sh_call(ctx, req);
    if (sh_ok(result)) {
        char name[64];
        sh_pid_name(ctx, pid, name, sizeof(name));
        sh_out(ctx, "%s pid=%d (%s)\n", verb, pid, name);
    } else {
        sh_out(ctx, "%s: %s\n", argv[0], sh_err(result));
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_memory(sh_ctx_t* ctx)
{
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "list_memory");
    cJSON* result = sh_call(ctx, req);
    sh_fmt_memory(ctx, result);
    cJSON_Delete(result);
    return 1;
}

static int cmd_alloc(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 3) {
        sh_out(ctx, "usage: alloc <pid> <pages>\n");
        return 1;
    }
    int pid = sh_pid_arg(ctx, argv[1]);
    int pages = 0;
    if (pid <= 0 || !sh_parse_int(argv[2], &pages) || pages <= 0) {
        if (pid > 0) {
            sh_out(ctx, "alloc: invalid page count\n");
        }
        return 1;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "mem_alloc");
    cJSON_AddNumberToObject(req, "pid", pid);
    cJSON_AddNumberToObject(req, "pages", pages);
    cJSON* result = sh_call(ctx, req);
    if (sh_ok(result)) {
        sh_out(ctx, "allocated %d page(s) for pid %d at vpage %d\n",
               (int)cJSON_GetObjectItemCaseSensitive(result, "pages")->valuedouble,
               pid, (int)cJSON_GetObjectItemCaseSensitive(result, "base_vpage")->valuedouble);
    } else {
        sh_out(ctx, "alloc: %s\n", sh_err(result));
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_free(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 2) {
        sh_out(ctx, "usage: free <pid>\n");
        return 1;
    }
    int pid = sh_pid_arg(ctx, argv[1]);
    if (pid <= 0) {
        return 1;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "mem_free");
    cJSON_AddNumberToObject(req, "pid", pid);
    cJSON* result = sh_call(ctx, req);
    if (sh_ok(result)) {
        sh_out(ctx, "freed %d page(s) for pid %d\n",
               (int)cJSON_GetObjectItemCaseSensitive(result, "pages_freed")->valuedouble, pid);
    } else {
        sh_out(ctx, "free: %s\n", sh_err(result));
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_mem_word(sh_ctx_t* ctx, int argc, char** argv, int write_word)
{
    if ((write_word && argc < 4) || (!write_word && argc < 3)) {
        sh_out(ctx, write_word ? "usage: write <pid> <addr> <value>\n"
                               : "usage: read <pid> <addr>\n");
        return 1;
    }
    int pid = sh_pid_arg(ctx, argv[1]);
    int addr = 0;
    int value = 0;
    if (pid <= 0 || !sh_parse_int(argv[2], &addr)) {
        if (pid > 0) {
            sh_out(ctx, "%s: invalid address\n", argv[0]);
        }
        return 1;
    }
    if (write_word && !sh_parse_int(argv[3], &value)) {
        sh_out(ctx, "write: invalid value\n");
        return 1;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", write_word ? "mem_write" : "mem_read");
    cJSON_AddNumberToObject(req, "pid", pid);
    cJSON_AddNumberToObject(req, "addr", addr);
    if (write_word) {
        cJSON_AddNumberToObject(req, "value", value);
    }
    cJSON* result = sh_call(ctx, req);
    if (!sh_ok(result)) {
        sh_out(ctx, "%s: %s\n", argv[0], sh_err(result));
    } else if (write_word) {
        sh_out(ctx, "pid %d [%d] = %d\n", pid, addr, value);
    } else {
        sh_out(ctx, "pid %d [%d] = %d\n", pid, addr,
               (int)cJSON_GetObjectItemCaseSensitive(result, "value")->valuedouble);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_sched(sh_ctx_t* ctx)
{
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "list_scheduler");
    cJSON* result = sh_call(ctx, req);
    if (!sh_ok(result)) {
        sh_out(ctx, "sched: %s\n", sh_err(result));
    } else {
        sh_out(ctx, "algo=%s current=%d switches=%d completed=%d "
                    "util=%.1f%% wait=%.2f turn=%.2f\n",
               cJSON_GetObjectItemCaseSensitive(result, "algo")->valuestring,
               (int)cJSON_GetObjectItemCaseSensitive(result, "current")->valuedouble,
               (int)cJSON_GetObjectItemCaseSensitive(result, "switches")->valuedouble,
               (int)cJSON_GetObjectItemCaseSensitive(result, "completed")->valuedouble,
               cJSON_GetObjectItemCaseSensitive(result, "cpu_utilization")->valuedouble,
               cJSON_GetObjectItemCaseSensitive(result, "avg_wait")->valuedouble,
               cJSON_GetObjectItemCaseSensitive(result, "avg_turnaround")->valuedouble);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_tick(sh_ctx_t* ctx, int argc, char** argv)
{
    int count = 1;
    if (argc > 1 && (!sh_parse_int(argv[1], &count) || count <= 0)) {
        sh_out(ctx, "usage: tick [count]\n");
        return 1;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "tick");
    cJSON_AddNumberToObject(req, "count", count);
    cJSON* result = sh_call(ctx, req);
    if (!sh_ok(result)) {
        sh_out(ctx, "tick: %s\n", sh_err(result));
    } else {
        sh_out(ctx, "advanced %d tick(s)\n", count);
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_devices(sh_ctx_t* ctx)
{
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "list_devices");
    cJSON* result = sh_call(ctx, req);
    sh_fmt_devices(ctx, result);
    cJSON_Delete(result);
    return 1;
}

static int cmd_ipc(sh_ctx_t* ctx)
{
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "list_ipc");
    cJSON* result = sh_call(ctx, req);
    sh_fmt_ipc(ctx, result);
    cJSON_Delete(result);
    return 1;
}

static int cmd_send(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 3) {
        sh_out(ctx, "usage: send <queue> <text>\n");
        return 1;
    }
    char text[SH_LINE_MAX];
    sh_join_argv(text, sizeof(text), argv, 2, argc);
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "ipc_queue_send");
    cJSON_AddStringToObject(req, "queue", argv[1]);
    cJSON_AddStringToObject(req, "data", text);
    cJSON_AddNumberToObject(req, "pid", 0);
    cJSON* result = sh_call(ctx, req);
    if (sh_ok(result)) {
        sh_out(ctx, "sent %d byte(s) to %s\n", (int)strlen(text), argv[1]);
    } else {
        sh_out(ctx, "send: %s\n", sh_err(result));
    }
    cJSON_Delete(result);
    return 1;
}

static int cmd_recv(sh_ctx_t* ctx, int argc, char** argv)
{
    if (argc < 2) {
        sh_out(ctx, "usage: recv <queue>\n");
        return 1;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "ipc_queue_recv");
    cJSON_AddStringToObject(req, "queue", argv[1]);
    cJSON_AddNumberToObject(req, "pid", 0);
    cJSON_AddBoolToObject(req, "nonblock", 1);
    cJSON* result = sh_call(ctx, req);
    if (sh_ok(result)) {
        const cJSON* data = cJSON_GetObjectItemCaseSensitive(result, "data");
        sh_out(ctx, "%s\n", cJSON_IsString(data) ? data->valuestring : "");
    } else {
        sh_out(ctx, "recv: %s\n", sh_err(result));
    }
    cJSON_Delete(result);
    return 1;
}

int sh_run_proc(sh_ctx_t* ctx, int argc, char** argv)
{
    const char* cmd = argv[0];
    if (strcmp(cmd, "ps") == 0) {
        return cmd_ps(ctx);
    }
    if (strcmp(cmd, "kill") == 0) {
        return cmd_pid_action(ctx, argc, argv, "kill_process", "killed");
    }
    if (strcmp(cmd, "suspend") == 0) {
        return cmd_pid_action(ctx, argc, argv, "suspend_process", "suspended");
    }
    if (strcmp(cmd, "resume") == 0) {
        return cmd_pid_action(ctx, argc, argv, "resume_process", "resumed");
    }
    if (sh_is(cmd, "memory", "mem")) {
        return cmd_memory(ctx);
    }
    if (strcmp(cmd, "alloc") == 0) {
        return cmd_alloc(ctx, argc, argv);
    }
    if (strcmp(cmd, "free") == 0) {
        return cmd_free(ctx, argc, argv);
    }
    if (strcmp(cmd, "write") == 0) {
        return cmd_mem_word(ctx, argc, argv, 1);
    }
    if (strcmp(cmd, "read") == 0) {
        return cmd_mem_word(ctx, argc, argv, 0);
    }
    if (strcmp(cmd, "sched") == 0) {
        return cmd_sched(ctx);
    }
    if (strcmp(cmd, "tick") == 0) {
        return cmd_tick(ctx, argc, argv);
    }
    if (strcmp(cmd, "devices") == 0) {
        return cmd_devices(ctx);
    }
    if (strcmp(cmd, "ipc") == 0) {
        return cmd_ipc(ctx);
    }
    if (strcmp(cmd, "send") == 0) {
        return cmd_send(ctx, argc, argv);
    }
    if (strcmp(cmd, "recv") == 0) {
        return cmd_recv(ctx, argc, argv);
    }
    return 0;
}
