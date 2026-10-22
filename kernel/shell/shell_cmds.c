/*
 * kernel/shell/shell_cmds.c
 *
 * JARVIS OS — shell dispatch and core commands (M8).
 */

#include <stdio.h>
#include <string.h>

#include "shell_internal.h"

void sh_init(jvk_shell_t* shell, sh_exec_fn exec, void* user)
{
    if (shell == NULL) {
        return;
    }
    snprintf(shell->cwd, sizeof(shell->cwd), "/");
    shell->last_status = 0;
    shell->exec = exec;
    shell->exec_user = user;
}

cJSON* sh_call(sh_ctx_t* ctx, cJSON* req)
{
    if (ctx == NULL || ctx->shell == NULL || ctx->shell->exec == NULL) {
        cJSON_Delete(req);
        return NULL;
    }
    cJSON* result = ctx->shell->exec(ctx->shell->exec_user, req);
    cJSON_Delete(req);
    return result;
}

int sh_ok(const cJSON* result)
{
    const cJSON* ok = cJSON_GetObjectItemCaseSensitive(result, "ok");
    return cJSON_IsBool(ok) && cJSON_IsTrue(ok);
}

const char* sh_err(const cJSON* result)
{
    const cJSON* err = cJSON_GetObjectItemCaseSensitive(result, "error");
    return cJSON_IsString(err) ? err->valuestring : "command failed";
}

int sh_pid_arg(sh_ctx_t* ctx, const char* text)
{
    int pid = 0;
    if (text != NULL && sh_parse_int(text, &pid)) {
        return pid > 0 ? pid : -1;
    }
    if (text == NULL) {
        return -1;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "list_processes");
    cJSON* result = sh_call(ctx, req);
    int found = -1;
    const cJSON* list = cJSON_GetObjectItemCaseSensitive(result, "processes");
    for (int i = 0; i < cJSON_GetArraySize(list); i++) {
        const cJSON* p = cJSON_GetArrayItem(list, i);
        const cJSON* name = cJSON_GetObjectItemCaseSensitive(p, "name");
        if (cJSON_IsString(name) && strcmp(name->valuestring, text) == 0) {
            const cJSON* id = cJSON_GetObjectItemCaseSensitive(p, "pid");
            found = cJSON_IsNumber(id) ? (int)id->valuedouble : -1;
            break;
        }
    }
    cJSON_Delete(result);
    if (found < 0) {
        sh_out(ctx, "no such process: %s\n", text);
    }
    return found;
}

void sh_pid_name(sh_ctx_t* ctx, int pid, char* out, size_t cap)
{
    snprintf(out, cap, "pid%d", pid);
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "action", "list_processes");
    cJSON* result = sh_call(ctx, req);
    const cJSON* list = cJSON_GetObjectItemCaseSensitive(result, "processes");
    for (int i = 0; i < cJSON_GetArraySize(list); i++) {
        const cJSON* p = cJSON_GetArrayItem(list, i);
        const cJSON* id = cJSON_GetObjectItemCaseSensitive(p, "pid");
        const cJSON* name = cJSON_GetObjectItemCaseSensitive(p, "name");
        if (cJSON_IsNumber(id) && (int)id->valuedouble == pid &&
            cJSON_IsString(name)) {
            snprintf(out, cap, "%s", name->valuestring);
            break;
        }
    }
    cJSON_Delete(result);
}

static void print_help(sh_ctx_t* ctx)
{
    sh_out(ctx,
        "JARVIS shell commands\n"
        "  help                     this text\n"
        "  ps                       process table and ready queue\n"
        "  kill <pid|name>          terminate a process\n"
        "  suspend/resume <pid>     stop or continue a process\n"
        "  memory | mem             memory statistics and page tables\n"
        "  alloc <pid> <pages>      allocate pages for a process\n"
        "  free <pid>               free all pages of a process\n"
        "  write <pid> <addr> <val> write one word of memory\n"
        "  read <pid> <addr>        read one word of memory\n"
        "  files                    show every inode\n"
        "  ls [path]                list a directory\n"
        "  mkdir <path>             create a directory\n"
        "  touch <path>             create an empty file\n"
        "  cat <path>               print a file\n"
        "  write-file <path> <text> write text to a file\n"
        "  rm <path>                delete a file or empty directory\n"
        "  mv <src> <dst>           move/rename\n"
        "  cp <src> <dst>           copy\n"
        "  chmod <path> <mode>      change permissions (e.g. 644)\n"
        "  cd [path] | pwd          change/show directory\n"
        "  sched                    scheduler state\n"
        "  tick [n]                 advance the kernel clock\n"
        "  devices                  device table\n"
        "  ipc                      IPC state\n"
        "  send <queue> <text>      send a message to a queue\n"
        "  recv <queue>             non-blocking receive\n"
        "  echo <text>              print text\n"
        "  version | whoami         identity\n"
        "  clear                    clear the terminal\n"
        "  shutdown                 power the kernel off\n");
}

int sh_run_core(sh_ctx_t* ctx, int argc, char** argv)
{
    const char* cmd = argv[0];
    if (sh_is(cmd, "help", "?")) {
        print_help(ctx);
        return 1;
    }
    if (strcmp(cmd, "echo") == 0) {
        char text[SH_LINE_MAX];
        sh_join_argv(text, sizeof(text), argv, 1, argc);
        sh_out(ctx, "%s\n", text);
        return 1;
    }
    if (strcmp(cmd, "clear") == 0) {
        ctx->clear = 1;
        return 1;
    }
    if (strcmp(cmd, "pwd") == 0) {
        sh_out(ctx, "%s\n", ctx->shell->cwd);
        return 1;
    }
    if (strcmp(cmd, "cd") == 0) {
        char path[SH_PATH_MAX];
        sh_join_path(path, sizeof(path), ctx->shell->cwd,
                     argc > 1 ? argv[1] : "/");
        cJSON* req = cJSON_CreateObject();
        cJSON_AddStringToObject(req, "action", "fs_stat");
        cJSON_AddStringToObject(req, "path", path);
        cJSON* result = sh_call(ctx, req);
        if (!sh_ok(result)) {
            sh_out(ctx, "cd: %s\n", sh_err(result));
        } else {
            const cJSON* entry = cJSON_GetObjectItemCaseSensitive(result, "entry");
            const cJSON* type = cJSON_GetObjectItemCaseSensitive(entry, "type");
            if (cJSON_IsString(type) && strcmp(type->valuestring, "dir") == 0) {
                snprintf(ctx->shell->cwd, sizeof(ctx->shell->cwd), "%s", path);
            } else {
                sh_out(ctx, "cd: not a directory: %s\n", path);
            }
        }
        cJSON_Delete(result);
        return 1;
    }
    if (sh_is(cmd, "version", NULL)) {
        cJSON* req = cJSON_CreateObject();
        cJSON_AddStringToObject(req, "action", "ping");
        cJSON* result = sh_call(ctx, req);
        sh_out(ctx, "JARVIS OS 1.0  kernel=%s  shell=M8\n",
               cJSON_IsString(cJSON_GetObjectItemCaseSensitive(result, "kernel"))
                   ? cJSON_GetObjectItemCaseSensitive(result, "kernel")->valuestring
                   : "c-native");
        cJSON_Delete(result);
        return 1;
    }
    if (strcmp(cmd, "whoami") == 0) {
        sh_out(ctx, "jarvis (uid 0, kernel shell)\n");
        return 1;
    }
    if (sh_is(cmd, "exit", "quit")) {
        ctx->exit = 1;
        sh_out(ctx, "logout\n");
        return 1;
    }
    if (strcmp(cmd, "shutdown") == 0) {
        cJSON* req = cJSON_CreateObject();
        cJSON_AddStringToObject(req, "action", "shutdown");
        cJSON* result = sh_call(ctx, req);
        if (sh_ok(result)) {
            sh_out(ctx, "system halted\n");
            ctx->exit = 1;
        } else {
            sh_out(ctx, "shutdown: %s\n", sh_err(result));
        }
        cJSON_Delete(result);
        return 1;
    }
    return 0;
}

int sh_execute(jvk_shell_t* shell, const char* line,
               char* out, size_t cap, int* clear, int* exit_requested)
{
    if (clear != NULL) {
        *clear = 0;
    }
    if (exit_requested != NULL) {
        *exit_requested = 0;
    }
    if (shell == NULL || line == NULL || out == NULL || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    char args[SH_MAX_ARGS][SH_ARG_LEN];
    int argc = sh_tokenize(line, args, SH_MAX_ARGS);
    if (argc < 0) {
        snprintf(out, cap, "parse error: too many arguments or unterminated quote\n");
        return 1;
    }
    if (argc == 0) {
        return 1;
    }
    char* argv[SH_MAX_ARGS];
    for (int i = 0; i < argc; i++) {
        argv[i] = args[i];
    }
    sh_ctx_t ctx = {shell, out, cap, 0, 0, 0};
    int found = sh_run_core(&ctx, argc, argv);
    if (!found) {
        found = sh_run_proc(&ctx, argc, argv);
    }
    if (!found) {
        found = sh_run_fs(&ctx, argc, argv);
    }
    if (!found) {
        sh_out(&ctx, "%s: command not found (try 'help')\n", argv[0]);
    }
    shell->last_status = found ? 0 : 127;
    if (clear != NULL) {
        *clear = ctx.clear;
    }
    if (exit_requested != NULL) {
        *exit_requested = ctx.exit;
    }
    return found;
}

void sh_prompt(const jvk_shell_t* shell, char* out, size_t cap)
{
    if (out == NULL || cap == 0) {
        return;
    }
    snprintf(out, cap, "jarvis@vos:%s$ ",
             shell != NULL && shell->cwd[0] != '\0' ? shell->cwd : "/");
}
