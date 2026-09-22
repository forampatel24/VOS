#include <stddef.h>
/*
 * kernel/memory/page_table.c
 *
 * JARVIS OS — per-process page table operations.
 */

#include <string.h>

#include "page_table.h"

void pt_init(mm_page_table_t* pt)
{
    if (pt == NULL) {
        return;
    }
    memset(pt, 0, sizeof(*pt));
    for (int i = 0; i < JVK_MM_PT_ENTRIES; i++) {
        pt->entries[i].swap_slot = -1;
    }
}

mm_pte_t* pt_entry(mm_page_table_t* pt, int vpage)
{
    if (pt == NULL || vpage < 0 || vpage >= JVK_MM_PT_ENTRIES) {
        return NULL;
    }
    return &pt->entries[vpage];
}

int pt_take_vpage(mm_page_table_t* pt)
{
    if (pt == NULL || pt->used_count >= JVK_MM_PT_ENTRIES) {
        return -1;
    }
    int vpage = pt->used_count;
    pt->used_count++;
    return vpage;
}

/* Virtual pages are only released wholesale (process exit), so release
   simply shrinks the compact space after the caller has cleaned bits. */
void pt_release_vpage(mm_page_table_t* pt, int vpage)
{
    if (pt == NULL || vpage != pt->used_count - 1) {
        return;
    }
    mm_pte_t* e = &pt->entries[vpage];
    memset(e, 0, sizeof(*e));
    e->swap_slot = -1;
    pt->used_count--;
}

void pt_clear(mm_page_table_t* pt)
{
    if (pt == NULL) {
        return;
    }
    int count = pt->used_count;
    pt_init(pt);
    pt->used_count = 0;
    (void)count;
}
