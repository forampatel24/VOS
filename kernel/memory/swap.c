#include <stddef.h>
/*
 * kernel/memory/swap.c
 *
 * JARVIS OS — simulated swap area operations.
 */

#include <stdlib.h>
#include <string.h>

#include "swap.h"

int swap_init(jvk_swap_t* s, int enabled, int capacity)
{
    if (s == NULL) {
        return 0;
    }
    memset(s, 0, sizeof(*s));
    if (capacity > JVK_MM_MAX_SWAP) {
        capacity = JVK_MM_MAX_SWAP;
    }
    s->enabled  = enabled ? 1 : 0;
    s->capacity = capacity;
    for (int i = 0; i < JVK_MM_MAX_SWAP; i++) {
        s->owner_pid[i] = 0;
    }
    if (!s->enabled) {
        return 1;
    }
    if (s->capacity > 0) {
        s->data = calloc((size_t)s->capacity, sizeof(*s->data));
        if (s->data == NULL) {
            return 0;
        }
    }
    return 1;
}

void swap_shutdown(jvk_swap_t* s)
{
    if (s == NULL) {
        return;
    }
    free(s->data);
    memset(s, 0, sizeof(*s));
}

int swap_put(jvk_swap_t* s, int pid, int vpage, const int* page_words)
{
    if (s == NULL || !s->enabled || pid <= 0 || page_words == NULL) {
        return -1;
    }
    for (int i = 0; i < s->capacity; i++) {
        if (s->owner_pid[i] != 0) {
            continue;
        }
        memcpy(s->data[i], page_words, sizeof(s->data[i]));
        s->owner_pid[i]   = pid;
        s->owner_vpage[i] = vpage;
        s->used++;
        return i;
    }
    return -1;
}

int swap_take(jvk_swap_t* s, int pid, int vpage, int* out_page_words)
{
    if (s == NULL || out_page_words == NULL || pid <= 0) {
        return 0;
    }
    for (int i = 0; i < s->capacity; i++) {
        if (s->owner_pid[i] == pid && s->owner_vpage[i] == vpage) {
            memcpy(out_page_words, s->data[i], sizeof(s->data[i]));
            s->owner_pid[i] = 0;
            s->used--;
            return 1;
        }
    }
    return 0;
}

int swap_used(const jvk_swap_t* s)
{
    return s != NULL ? s->used : 0;
}

void swap_release_pid(jvk_swap_t* s, int pid)
{
    if (s == NULL || pid <= 0) {
        return;
    }
    for (int i = 0; i < s->capacity; i++) {
        if (s->owner_pid[i] == pid) {
            s->owner_pid[i] = 0;
            s->used--;
        }
    }
}
