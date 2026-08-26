/*
 * kernel/memory/memory_manager.h
 *
 * JARVIS OS — virtual memory authority.
 *
 * The memory manager owns physical frames, per-process page tables,
 * placement strategy and replacement policy. Every allocation updates
 * the page tables, the frame table, statistics and emits an event —
 * never a subset. All access is through virtual addresses translated
 * against the caller's page table; a bad address is a segfault, never
 * silent corruption.
 *
 * Policies are interchangeable strategies selected at boot (config)
 * or at runtime (mem_config).
 */

#ifndef JARVIS_MEMORY_MANAGER_H
#define JARVIS_MEMORY_MANAGER_H

#include "alloc_strategy.h"
#include "frame_table.h"
#include "mm_types.h"
#include "page_table.h"
#include "replace_policy.h"
#include "swap.h"

/* Event observer: the kernel subscribes so MEMORY_* / PAGE_FAULT /
   SWAP_* events land in the central log. */
typedef void (*mm_event_fn)(void* user, const char* message);

typedef struct {
    int                 total_frames;
    mm_alloc_strategy_t allocator;
    mm_replace_policy_t replacement;
    int                 swap_enabled;
    int                 swap_slots;
} mm_config_t;

typedef struct {
    int base_vpage;
    int count;
    int contiguous; /* 1 when placed as one physical run */
    int frames[JVK_MM_PT_ENTRIES];
} mm_alloc_result_t;

typedef struct {
    mm_config_t      cfg;
    mm_frame_table_t ft;
    jvk_swap_t       swap;
    mm_page_table_t  tables[JVK_MM_MAX_PROCS];
    int              pids[JVK_MM_MAX_PROCS]; /* 0 = free slot */
    int            (*data)[JVK_MM_PAGE_WORDS]; /* heap: frame storage */
    unsigned long    seq;        /* monotonic FIFO/LRU stamp */
    int              clock_hand;
    mm_event_fn      on_event;
    void*            event_user;
    /* statistics */
    long allocs;
    long frees;
    long page_faults;
    long swap_ins;
    long swap_outs;
    long segfaults;
    long fragmented_allocs;
    long pages_mapped;
} jvk_memory_manager_t;

int  mm_init(jvk_memory_manager_t* mm, const mm_config_t* cfg);
void mm_shutdown(jvk_memory_manager_t* mm);
void mm_set_observer(jvk_memory_manager_t* mm, mm_event_fn fn, void* user);

/* Runtime strategy switching. Names use the canonical strings
   ("first_fit".."worst_fit", "fifo"/"lru"/"clock"). */
int  mm_set_allocator(jvk_memory_manager_t* mm, const char* name);
int  mm_set_replacement(jvk_memory_manager_t* mm, const char* name);

mm_status_t mm_alloc(jvk_memory_manager_t* mm, int pid, int npages,
                     mm_alloc_result_t* out);
/* Returns pages freed, or -1 when the pid has no address space. */
int         mm_free_pid(jvk_memory_manager_t* mm, int pid);
mm_status_t mm_read(jvk_memory_manager_t* mm, int pid, int addr,
                    int* out_value);
mm_status_t mm_write(jvk_memory_manager_t* mm, int pid, int addr,
                     int value);

#endif /* JARVIS_MEMORY_MANAGER_H */
