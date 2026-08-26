/*
 * kernel/memory/mm_types.h
 *
 * JARVIS OS — shared memory-subsystem types.
 *
 * One virtual page maps to one physical frame. Frames hold
 * JVK_MM_PAGE_WORDS integer words. Every process owns one page table;
 * the frame table is global. Policies (allocation strategy, page
 * replacement) are runtime-selectable strategies, never hardcoded.
 */

#ifndef JARVIS_MM_TYPES_H
#define JARVIS_MM_TYPES_H

#include <stdint.h>

#define JVK_MM_MAX_FRAMES  1024 /* physical frames cap */
#define JVK_MM_PAGE_WORDS  16   /* integer words per page */
#define JVK_MM_MAX_PROCS   8    /* mirrors JVK_MAX_PROCS */
#define JVK_MM_PT_ENTRIES  64   /* virtual pages per process */
#define JVK_MM_MAX_SWAP    512  /* swap slots cap */

/* ---- allocation strategies (contiguous-run placement) ---------------- */

typedef enum {
    MM_ALLOC_FIRST_FIT = 0,
    MM_ALLOC_BEST_FIT,
    MM_ALLOC_WORST_FIT,
} mm_alloc_strategy_t;

const char* mm_alloc_name(mm_alloc_strategy_t s);
int         mm_alloc_parse(const char* name, mm_alloc_strategy_t* out);

/* ---- page replacement policies --------------------------------------- */

typedef enum {
    MM_REPLACE_FIFO = 0,
    MM_REPLACE_LRU,
    MM_REPLACE_CLOCK,
} mm_replace_policy_t;

const char* mm_replace_name(mm_replace_policy_t p);
int         mm_replace_parse(const char* name, mm_replace_policy_t* out);

/* ---- status codes ----------------------------------------------------- */

typedef enum {
    MM_OK = 0,
    MM_ERR_ARG,          /* bad argument / config */
    MM_ERR_NO_PID,       /* no address space for pid */
    MM_ERR_OOM,          /* out of memory (and swap cannot help) */
    MM_ERR_SEGV,         /* access to an unmapped address */
} mm_status_t;

const char* mm_status_name(mm_status_t s);

#endif /* JARVIS_MM_TYPES_H */
