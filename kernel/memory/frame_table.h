/*
 * kernel/memory/frame_table.h
 *
 * JARVIS OS — global physical frame table.
 *
 * One entry per physical frame records ownership (owner pid + virtual
 * page) and the metadata every replacement policy needs: load order
 * (FIFO), last access (LRU) and the clock reference bit. A monotonic
 * sequence number stamped by the manager drives FIFO / LRU fairly.
 */

#ifndef JARVIS_FRAME_TABLE_H
#define JARVIS_FRAME_TABLE_H

#include "mm_types.h"

typedef enum {
    MM_FRAME_FREE = 0,
    MM_FRAME_USED,
} mm_frame_state_t;

typedef struct {
    int           state;
    int           owner_pid;   /* valid when USED */
    int           vpage;       /* virtual page mapped here */
    unsigned long loaded_seq;  /* FIFO stamp */
    unsigned long access_seq;  /* LRU stamp */
    unsigned      ref_bit;     /* clock hint */
} mm_frame_t;

typedef struct {
    int        total;
    mm_frame_t frames[JVK_MM_MAX_FRAMES];
} mm_frame_table_t;

void          ft_init(mm_frame_table_t* ft, int total_frames);
int           ft_free_count(const mm_frame_table_t* ft);
mm_frame_t*   ft_at(mm_frame_table_t* ft, int frame);
void          ft_mark_used(mm_frame_table_t* ft, int frame, int pid,
                           int vpage, unsigned long seq);
void          ft_mark_free(mm_frame_table_t* ft, int frame);
/* First free frame at or after `from` (wraps once); -1 when none. */
int           ft_scan_free(const mm_frame_table_t* ft, int from);
/* Longest run of consecutive FREE frames starting at each index. */
int           ft_run_length(const mm_frame_table_t* ft, int start);

#endif /* JARVIS_FRAME_TABLE_H */
