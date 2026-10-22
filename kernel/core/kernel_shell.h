/*
 * kernel/core/kernel_shell.h
 *
 * JARVIS OS — kernel glue for the M8 shell.
 *
 * The shell is a pure consumer of the command dispatcher: its exec
 * callback re-enters jvk_command so every filesystem/memory/IPC effect
 * still goes through the kernel's ABI path.
 */

#ifndef JARVIS_KERNEL_SHELL_H
#define JARVIS_KERNEL_SHELL_H

#include "../deps/cJSON/cJSON.h"
#include "../shell/shell.h"

typedef struct {
    jvk_shell_t shell;
    char        last_output[512];
} kshell_t;

void ksh_boot(kshell_t* ksh);

/* Handle "shell_exec"; returns 1 if consumed. */
int  ksh_handle(kshell_t* ksh, const char* action,
                const cJSON* req, cJSON* result);

void ksh_snapshot(const kshell_t* ksh, cJSON* root);

#endif /* JARVIS_KERNEL_SHELL_H */
