/*
 * kernel/memory/page_table.h
 *
 * JARVIS OS — per-process page table.
 *
 * A page table owns up to JVK_MM_PT_ENTRIES virtual pages. Virtual page
 * numbers are handed out sequentially and compactly: a process's address
 * space is [0 .. used_count). Each entry carries the classic residency,
 * reference and dirty bits plus the swap slot used while the page is
 * swapped out.
 */

#ifndef JARVIS_PAGE_TABLE_H
#define JARVIS_PAGE_TABLE_H

#include "mm_types.h"

typedef struct {
    int      present;    /* resident in a physical frame */
    int      frame;      /* valid when present */
    int      swap_slot;  /* valid when !present; -1 = never swapped */
    unsigned referenced; /* touched since last scan */
    unsigned dirty;      /* written at least once */
} mm_pte_t;

typedef struct {
    int      used_count; /* next virtual page to hand out */
    mm_pte_t entries[JVK_MM_PT_ENTRIES];
} mm_page_table_t;

void      pt_init(mm_page_table_t* pt);
mm_pte_t* pt_entry(mm_page_table_t* pt, int vpage);
int       pt_take_vpage(mm_page_table_t* pt); /* next vpage index or -1 */
void      pt_release_vpage(mm_page_table_t* pt, int vpage);
void      pt_clear(mm_page_table_t* pt);

#endif /* JARVIS_PAGE_TABLE_H */
