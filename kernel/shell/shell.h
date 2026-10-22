/*
 * kernel/shell/shell.h
 *
 * JARVIS OS — shell core (M8).
 *
 * The shell parses a line, runs one command and writes human-readable
 * text. It never touches a subsystem directly: every kernel operation
 * goes back through the injected sh_exec_fn (the kernel dispatcher).
 */

#ifndef JARVIS_SHELL_H
#define JARVIS_SHELL_H

#include <stddef.h>

#include "../deps/cJSON/cJSON.h"
#include "shell_parse.h"

/* Send one kernel request; returns a freshly allocated result object. */
typedef cJSON* (*sh_exec_fn)(void* user, const cJSON* request);

typedef struct {
    char        cwd[SH_PATH_MAX];
    int         last_status;
    sh_exec_fn  exec;
    void*       exec_user;
} jvk_shell_t;

void sh_init(jvk_shell_t* shell, sh_exec_fn exec, void* user);

/* Execute one line. Returns 1 when a command was found and run. */
int  sh_execute(jvk_shell_t* shell, const char* line,
                char* out, size_t cap, int* clear, int* exit_requested);

/* "jarvis@vos:/path$ " */
void sh_prompt(const jvk_shell_t* shell, char* out, size_t cap);

#endif /* JARVIS_SHELL_H */
