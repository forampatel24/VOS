/*
 * kernel/shell/shell_parse.h
 *
 * JARVIS OS — shell tokenizer and path helpers (M8).
 */

#ifndef JARVIS_SHELL_PARSE_H
#define JARVIS_SHELL_PARSE_H

#include <stddef.h>

#define SH_MAX_ARGS  16
#define SH_ARG_LEN   96
#define SH_LINE_MAX  256
#define SH_PATH_MAX  192

/* Split a command line on whitespace, honouring double quotes.
   Returns the argument count, or -1 on malformed input. */
int sh_tokenize(const char* line, char args[][SH_ARG_LEN], int max_args);

/* Strict decimal parse; returns 1 on success. */
int sh_parse_int(const char* text, int* out);

/* Join argv[from..argc) with single spaces. */
void sh_join_argv(char* out, size_t cap, char* const* argv,
                  int from, int argc);

/* Resolve arg against cwd, collapsing "." and "..". Always absolute. */
int sh_join_path(char* out, size_t cap, const char* cwd, const char* arg);

/* strcmp that also accepts a one-word alias (alias may be NULL). */
int sh_is(const char* text, const char* full, const char* alias);

/* Trim leading/trailing whitespace in place; returns the new start. */
char* sh_trim(char* text);

#endif /* JARVIS_SHELL_PARSE_H */
