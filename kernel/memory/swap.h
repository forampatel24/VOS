/*
 * kernel/memory/swap.h
 *
 * JARVIS OS — simulated swap area.
 *
 * Backing store for evicted anonymous pages. Each slot holds the full
 * contents of one page plus its identity (owner pid + virtual page) so
 * a swapped-in access can be matched to its saved copy.
 */

#ifndef JARVIS_SWAP_H
#define JARVIS_SWAP_H

#include "mm_types.h"

typedef struct {
    int          enabled;
    int          capacity;
    int          used;
    int        (*data)[JVK_MM_PAGE_WORDS];   /* heap: capacity pages */
    int          owner_pid[JVK_MM_MAX_SWAP]; /* 0 = free slot */
    int          owner_vpage[JVK_MM_MAX_SWAP];
} jvk_swap_t;

int  swap_init(jvk_swap_t* s, int enabled, int capacity);
void swap_shutdown(jvk_swap_t* s);

/* Save a page into a free slot; returns the slot index or -1 when full
   or disabled. The caller must have verified capacity beforehand. */
int  swap_put(jvk_swap_t* s, int pid, int vpage, const int* page_words);

/* Restore the saved copy of (pid, vpage) into out_page_words and free
   the slot. Returns 1 on success, 0 when no matching copy exists. */
int  swap_take(jvk_swap_t* s, int pid, int vpage, int* out_page_words);

int  swap_used(const jvk_swap_t* s);
void swap_release_pid(jvk_swap_t* s, int pid); /* process teardown */

#endif /* JARVIS_SWAP_H */
