/*
 * kernel/core/kernel_shell.c
 *
 * JARVIS OS — shell execution glue (M8).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../abi.h"
#include "kernel_shell.h"

static cJSON* shell_exec(void* user, const cJSON* request)
{
    (void)user;
    char* line = cJSON_PrintUnformatted((cJSON*)request);
    if (line == NULL) {
        return NULL;
    }
    const char* raw = jvk_command(line);
    cJSON* out = cJSON_Parse(raw);
    free(line);
    if (out == NULL) {
        out = cJSON_CreateObject();
        cJSON_AddBoolToObject(out, "ok", 0);
        cJSON_AddStringToObject(out, "error", "kernel reply parse failed");
    }
    return out;
}

void ksh_boot(kshell_t* ksh)
{
    if (ksh == NULL) {
        return;
    }
    sh_init(&ksh->shell, shell_exec, NULL);
    ksh->last_output[0] = '\0';
}

static void add_prompt(kshell_t* ksh, cJSON* result)
{
    char prompt[SH_PATH_MAX + 32];
    sh_prompt(&ksh->shell, prompt, sizeof(prompt));
    cJSON_AddStringToObject(result, "prompt", prompt);
    cJSON_AddStringToObject(result, "cwd", ksh->shell.cwd);
}

int ksh_handle(kshell_t* ksh, const char* action,
               const cJSON* req, cJSON* result)
{
    if (ksh == NULL || action == NULL || strcmp(action, "shell_exec") != 0) {
        return 0;
    }
    const cJSON* line_item = cJSON_GetObjectItemCaseSensitive(req, "line");
    if (!cJSON_IsString(line_item)) {
        line_item = cJSON_GetObjectItemCaseSensitive(req, "command");
    }
    const char* line = cJSON_IsString(line_item) ? line_item->valuestring : "";
    char output[2048];
    output[0] = '\0';
    int clear = 0;
    int exit_requested = 0;
    int found = sh_execute(&ksh->shell, line, output, sizeof(output),
                           &clear, &exit_requested);
    snprintf(ksh->last_output, sizeof(ksh->last_output), "%s", output);

    cJSON_AddBoolToObject(result, "ok", found ? 1 : 0);
    cJSON_AddStringToObject(result, "output", output);
    cJSON_AddBoolToObject(result, "clear", clear ? 1 : 0);
    cJSON_AddBoolToObject(result, "exit", exit_requested ? 1 : 0);
    cJSON_AddStringToObject(result, "command", line);
    add_prompt(ksh, result);
    if (!found) {
        cJSON_AddStringToObject(result, "error", "command not found");
    }
    return 1;
}

void ksh_snapshot(const kshell_t* ksh, cJSON* root)
{
    if (ksh == NULL || root == NULL) {
        return;
    }
    cJSON* shell = cJSON_CreateObject();
    char prompt[SH_PATH_MAX + 32];
    sh_prompt(&ksh->shell, prompt, sizeof(prompt));
    cJSON_AddStringToObject(shell, "cwd", ksh->shell.cwd);
    cJSON_AddStringToObject(shell, "prompt", prompt);
    cJSON_AddNumberToObject(shell, "last_status", ksh->shell.last_status);
    cJSON_AddStringToObject(shell, "last_output", ksh->last_output);
    cJSON_AddItemToObject(root, "shell", shell);
}
