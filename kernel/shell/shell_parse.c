/*
 * kernel/shell/shell_parse.c
 *
 * JARVIS OS — shell tokenizer and path helpers (M8).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell_parse.h"

int sh_tokenize(const char* line, char args[][SH_ARG_LEN], int max_args)
{
    if (line == NULL || args == NULL || max_args <= 0) {
        return -1;
    }
    int count = 0;
    const char* p = line;
    while (*p != '\0') {
        while (*p != '\0' && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (count >= max_args) {
            return -1;
        }
        int len = 0;
        if (*p == '"') {
            p++;
            while (*p != '\0' && *p != '"') {
                if (len >= SH_ARG_LEN - 1) {
                    return -1;
                }
                args[count][len++] = *p++;
            }
            if (*p != '"') {
                return -1; /* unterminated quote */
            }
            p++;
        } else {
            while (*p != '\0' && !isspace((unsigned char)*p)) {
                if (len >= SH_ARG_LEN - 1) {
                    return -1;
                }
                args[count][len++] = *p++;
            }
        }
        args[count][len] = '\0';
        count++;
    }
    return count;
}

int sh_parse_int(const char* text, int* out)
{
    if (text == NULL || *text == '\0' || out == NULL) {
        return 0;
    }
    char* end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = (int)value;
    return 1;
}

void sh_join_argv(char* out, size_t cap, char* const* argv,
                  int from, int argc)
{
    if (out == NULL || cap == 0) {
        return;
    }
    out[0] = '\0';
    size_t used = 0;
    for (int i = from; i < argc; i++) {
        const char* word = argv[i] != NULL ? argv[i] : "";
        size_t need = strlen(word) + (used > 0 ? 1 : 0);
        if (used + need + 1 > cap) {
            break;
        }
        if (used > 0) {
            out[used++] = ' ';
        }
        memcpy(out + used, word, strlen(word));
        used += strlen(word);
        out[used] = '\0';
    }
}

static int append_component(char* out, size_t cap, size_t* used,
                            const char* text, size_t len)
{
    if (len == 0) {
        return 1;
    }
    if (len == 1 && text[0] == '.') {
        return 1;
    }
    if (len == 2 && text[0] == '.' && text[1] == '.') {
        while (*used > 0 && out[*used - 1] != '/') {
            (*used)--;
        }
        if (*used > 0) {
            (*used)--;
        }
        out[*used] = '\0';
        return 1;
    }
    if (*used + len + 2 > cap) {
        return 0;
    }
    out[(*used)++] = '/';
    memcpy(out + *used, text, len);
    *used += len;
    out[*used] = '\0';
    return 1;
}

static int normalize_into(char* out, size_t cap, const char* src)
{
    size_t used = 0;
    out[0] = '\0';
    const char* p = src;
    while (*p != '\0') {
        while (*p == '/') {
            p++;
        }
        const char* start = p;
        while (*p != '\0' && *p != '/') {
            p++;
        }
        if (!append_component(out, cap, &used, start, (size_t)(p - start))) {
            return 0;
        }
    }
    if (used == 0) {
        snprintf(out, cap, "/");
    }
    return 1;
}

int sh_join_path(char* out, size_t cap, const char* cwd, const char* arg)
{
    if (out == NULL || cap == 0) {
        return 0;
    }
    if (arg == NULL || arg[0] == '\0') {
        snprintf(out, cap, "%s", cwd != NULL && cwd[0] != '\0' ? cwd : "/");
        return 1;
    }
    char combined[SH_PATH_MAX * 2];
    if (arg[0] == '/') {
        snprintf(combined, sizeof(combined), "%s", arg);
    } else {
        snprintf(combined, sizeof(combined), "%s/%s",
                 cwd != NULL && cwd[0] != '\0' ? cwd : "/", arg);
    }
    char normalized[SH_PATH_MAX];
    if (!normalize_into(normalized, sizeof(normalized), combined)) {
        return 0;
    }
    snprintf(out, cap, "%s", normalized);
    return 1;
}

int sh_is(const char* text, const char* full, const char* alias)
{
    if (text == NULL || full == NULL) {
        return 0;
    }
    return strcmp(text, full) == 0 || (alias != NULL && strcmp(text, alias) == 0);
}

char* sh_trim(char* text)
{
    if (text == NULL) {
        return NULL;
    }
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }
    return text;
}
