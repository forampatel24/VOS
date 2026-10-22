/*
 * kernel/shell/shell_internal.h
 *
 * Shared contexts and formatters between the shell command files (M8).
 */

#ifndef JARVIS_SHELL_INTERNAL_H
#define JARVIS_SHELL_INTERNAL_H

#include "shell.h"

typedef struct {
    jvk_shell_t* shell;
    char*        out;
    size_t       cap;
    size_t       len;
    int          clear;
    int          exit;
} sh_ctx_t;

/* Append formatted text to the output buffer. */
void sh_out(sh_ctx_t* ctx, const char* fmt, ...);

/* Print request, call the kernel dispatcher and parse its reply.
   Takes ownership of req; returns a new cJSON object (or NULL). */
cJSON* sh_call(sh_ctx_t* ctx, cJSON* req);

int         sh_ok(const cJSON* result);
const char* sh_err(const cJSON* result);

/* Resolve a pid argument: digits or a live process name. */
int  sh_pid_arg(sh_ctx_t* ctx, const char* text);
void sh_pid_name(sh_ctx_t* ctx, int pid, char* out, size_t cap);

/* Command groups; each returns 1 when the command was handled. */
int sh_run_core(sh_ctx_t* ctx, int argc, char** argv);
int sh_run_proc(sh_ctx_t* ctx, int argc, char** argv);
int sh_run_fs(sh_ctx_t* ctx, int argc, char** argv);

/* Result formatters. */
void sh_fmt_processes(sh_ctx_t* ctx, const cJSON* result);
void sh_fmt_memory(sh_ctx_t* ctx, const cJSON* result);
void sh_fmt_entries(sh_ctx_t* ctx, const cJSON* result);
void sh_fmt_devices(sh_ctx_t* ctx, const cJSON* result);
void sh_fmt_ipc(sh_ctx_t* ctx, const cJSON* result);

#endif /* JARVIS_SHELL_INTERNAL_H */
