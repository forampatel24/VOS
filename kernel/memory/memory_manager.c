#include <stddef.h>
/*
 * kernel/memory/memory_manager.c
 *
 * JARVIS OS — virtual memory authority (implementation).
 *
 * Flow of an allocation:
 *   validate -> gather frames (contiguous run by strategy, else
 *   scattered fallback = external fragmentation) -> evict via the
 *   replacement policy when pressure demands -> map PTEs -> zero fill
 *   -> stats + event.
 *
 * Flow of an access:
 *   translate vaddr -> page not resident ? page fault + swap-in
 *   -> touch stamps (LRU/FIFO/clock metadata) -> read / write word.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory_manager.h"

#define MM_EVENT_LEN 128

static void emit(jvk_memory_manager_t* mm, const char* fmt, ...)
{
    char buf[MM_EVENT_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (mm->on_event != NULL) {
        mm->on_event(mm->event_user, buf);
    }
}

/* ---- address-space table --------------------------------------------- */

static int table_index(const jvk_memory_manager_t* mm, int pid)
{
    for (int i = 0; i < JVK_MM_MAX_PROCS; i++) {
        if (mm->pids[i] == pid) {
            return i;
        }
    }
    return -1;
}

static int table_create(jvk_memory_manager_t* mm, int pid)
{
    int slot = -1;
    for (int i = 0; i < JVK_MM_MAX_PROCS && slot < 0; i++) {
        if (mm->pids[i] == 0) {
            slot = i;
        }
    }
    if (slot >= 0) {
        mm->pids[slot] = pid;
    }
    return slot;
}

static mm_page_table_t* table_of(jvk_memory_manager_t* mm, int pid)
{
    int idx = table_index(mm, pid);
    return idx >= 0 ? &mm->tables[idx] : NULL;
}

/* ---- frame gathering --------------------------------------------------- */

static int gather_free(jvk_memory_manager_t* mm, int need,
                       int* out_frames, int* out_contig)
{
    *out_contig = 0;
    int start = as_find_contiguous(&mm->ft, mm->cfg.allocator, need);
    if (start >= 0) {
        for (int k = 0; k < need; k++) {
            out_frames[k] = start + k;
        }
        *out_contig = 1;
        return need;
    }
    return as_collect_scattered(&mm->ft, need, out_frames, need);
}

static void claim_frames(jvk_memory_manager_t* mm, int pid,
                         const int* frames, int count)
{
    for (int k = 0; k < count; k++) {
        ft_mark_used(&mm->ft, frames[k], pid, -1, ++mm->seq);
    }
}

/* Evict residents until at least `need` frames are free. Returns the
   number still missing (0 on success). */
static int reclaim_frames(jvk_memory_manager_t* mm, int need)
{
    if (!mm->cfg.swap_enabled) {
        return ft_free_count(&mm->ft) >= need ? 0 : need;
    }
    while (ft_free_count(&mm->ft) < need) {
        int victim = rp_pick_victim(&mm->ft, mm->cfg.replacement,
                                    &mm->clock_hand);
        if (victim < 0) {
            break;
        }
        mm_frame_t* f     = ft_at(&mm->ft, victim);
        int         vpid  = f->owner_pid;
        int         vpage = f->vpage;

        int slot = swap_put(&mm->swap, vpid, vpage, mm->data[victim]);
        if (slot < 0) {
            break; /* swap full: cannot free more frames */
        }
        mm_pte_t* e = pt_entry(table_of(mm, vpid), vpage);
        if (e != NULL) {
            e->present   = 0;
            e->frame     = -1;
            e->swap_slot = slot;
        }
        ft_mark_free(&mm->ft, victim);
        mm->swap_outs++;
        emit(mm, "FRAME_RECLAIM frame=%d from_pid=%d", victim, vpid);
        emit(mm, "SWAP_OUT pid=%d vpage=%d slot=%d", vpid, vpage, slot);
    }
    return ft_free_count(&mm->ft) >= need ? 0 : need;
}

/* ---- public API -------------------------------------------------------- */

int mm_init(jvk_memory_manager_t* mm, const mm_config_t* cfg)
{
    if (mm == NULL || cfg == NULL || cfg->total_frames < 1 ||
        cfg->total_frames > JVK_MM_MAX_FRAMES) {
        return 0;
    }
    memset(mm, 0, sizeof(*mm));
    mm->cfg = *cfg;

    ft_init(&mm->ft, cfg->total_frames);
    if (!swap_init(&mm->swap, cfg->swap_enabled, cfg->swap_slots)) {
        return 0;
    }
    if (cfg->total_frames > 0) {
        mm->data = calloc((size_t)cfg->total_frames, sizeof(*mm->data));
        if (mm->data == NULL) {
            swap_shutdown(&mm->swap);
            return 0;
        }
    }
    for (int i = 0; i < JVK_MM_MAX_PROCS; i++) {
        pt_init(&mm->tables[i]);
        mm->pids[i] = 0;
    }
    return 1;
}

void mm_shutdown(jvk_memory_manager_t* mm)
{
    if (mm == NULL) {
        return;
    }
    swap_shutdown(&mm->swap);
    free(mm->data);
    mm->data = NULL;
}

void mm_set_observer(jvk_memory_manager_t* mm, mm_event_fn fn, void* user)
{
    if (mm != NULL) {
        mm->on_event   = fn;
        mm->event_user = user;
    }
}

int mm_set_allocator(jvk_memory_manager_t* mm, const char* name)
{
    mm_alloc_strategy_t s;
    if (mm == NULL || !mm_alloc_parse(name, &s)) {
        return 0;
    }
    mm->cfg.allocator = s;
    return 1;
}

int mm_set_replacement(jvk_memory_manager_t* mm, const char* name)
{
    mm_replace_policy_t p;
    if (mm == NULL || !mm_replace_parse(name, &p)) {
        return 0;
    }
    mm->cfg.replacement = p;
    return 1;
}

mm_status_t mm_alloc(jvk_memory_manager_t* mm, int pid, int npages,
                     mm_alloc_result_t* out)
{
    if (mm == NULL || out == NULL || pid <= 0 || npages <= 0) {
        return MM_ERR_ARG;
    }
    mm_page_table_t* pt = table_of(mm, pid);
    if (pt == NULL) {
        int slot = table_create(mm, pid);
        if (slot < 0) {
            return MM_ERR_NO_PID;
        }
        pt = &mm->tables[slot];
    }
    if (pt->used_count + npages > JVK_MM_PT_ENTRIES) {
        return MM_ERR_ARG;
    }

    int frames[JVK_MM_PT_ENTRIES];
    int contig = 0;
    int got    = gather_free(mm, npages, frames, &contig);
    if (got < npages) {
        /* gather re-scans every free frame, so the retry only helps if
           the TOTAL free count reaches the request size: make that the
           eviction target. */
        if (reclaim_frames(mm, npages) != 0) {
            return MM_ERR_OOM;
        }
        got = gather_free(mm, npages, frames, &contig);
        if (got < npages) {
            return MM_ERR_OOM;
        }
    }
    if (!contig) {
        mm->fragmented_allocs++;
        emit(mm, "EXTERNAL_FRAGMENTATION pid=%d pages=%d", pid, npages);
    }

    claim_frames(mm, pid, frames, npages);
    out->base_vpage = pt->used_count;
    out->count      = npages;
    out->contiguous = contig;
    for (int k = 0; k < npages; k++) {
        int vpage = pt_take_vpage(pt);
        memset(mm->data[frames[k]], 0, sizeof(mm->data[frames[k]]));
        ft_at(&mm->ft, frames[k])->vpage = vpage;
        mm_pte_t* e = pt_entry(pt, vpage);
        e->present   = 1;
        e->frame     = frames[k];
        e->swap_slot = -1;
        e->referenced = 1;
        e->dirty      = 0;
        out->frames[vpage - out->base_vpage] = frames[k];
    }
    mm->allocs++;
    mm->pages_mapped += npages;
    emit(mm, "MEMORY_ALLOCATED pid=%d pages=%d base_vpage=%d",
         pid, npages, out->base_vpage);
    return MM_OK;
}

int mm_free_pid(jvk_memory_manager_t* mm, int pid)
{
    if (mm == NULL || pid <= 0) {
        return -1;
    }
    int idx = table_index(mm, pid);
    if (idx < 0) {
        return -1;
    }
    mm_page_table_t* pt = &mm->tables[idx];
    int              freed = 0;
    for (int vpage = 0; vpage < pt->used_count; vpage++) {
        mm_pte_t* e = pt_entry(pt, vpage);
        if (e->present) {
            ft_mark_free(&mm->ft, e->frame);
            freed++;
        } else if (e->swap_slot >= 0) {
            freed++;
        }
    }
    swap_release_pid(&mm->swap, pid);
    pt_clear(pt);
    mm->pids[idx] = 0;
    if (freed > 0) {
        mm->frees++;
        mm->pages_mapped -= freed;
        emit(mm, "MEMORY_FREED pid=%d pages=%d", pid, freed);
    }
    return freed;
}

/* Resolve a virtual address to a resident frame, faulting in the page
   when it sits in swap. Returns MM_OK or an error code. */
static mm_status_t resolve(jvk_memory_manager_t* mm, int pid, int addr,
                           mm_page_table_t** out_pt, int* out_frame,
                           int* out_offset)
{
    mm_page_table_t* pt = table_of(mm, pid);
    if (pt == NULL) {
        return MM_ERR_NO_PID;
    }
    if (addr < 0) {
        mm->segfaults++;
        emit(mm, "SEGV pid=%d addr=%d", pid, addr);
        return MM_ERR_SEGV;
    }
    int vpage  = addr / JVK_MM_PAGE_WORDS;
    int offset = addr % JVK_MM_PAGE_WORDS;
    if (vpage >= pt->used_count) {
        mm->segfaults++;
        emit(mm, "SEGV pid=%d addr=%d", pid, addr);
        return MM_ERR_SEGV;
    }

    mm_pte_t* e = pt_entry(pt, vpage);
    if (!e->present) {
        mm->page_faults++;
        emit(mm, "PAGE_FAULT pid=%d vpage=%d", pid, vpage);
        int frame = ft_scan_free(&mm->ft, 0);
        if (frame < 0 && reclaim_frames(mm, 1) == 0) {
            frame = ft_scan_free(&mm->ft, 0);
        }
        if (frame < 0) {
            return MM_ERR_OOM;
        }
        ft_mark_used(&mm->ft, frame, pid, vpage, ++mm->seq);
        memset(mm->data[frame], 0, sizeof(mm->data[frame]));
        swap_take(&mm->swap, pid, vpage, mm->data[frame]);
        e->present   = 1;
        e->frame     = frame;
        e->swap_slot = -1;
        mm->swap_ins++;
        emit(mm, "SWAP_IN pid=%d vpage=%d frame=%d", pid, vpage, frame);
    }

    mm_frame_t* f = ft_at(&mm->ft, e->frame);
    f->access_seq = ++mm->seq;
    f->ref_bit    = 1;
    *out_pt      = pt;
    *out_frame   = e->frame;
    *out_offset  = offset;
    return MM_OK;
}

mm_status_t mm_read(jvk_memory_manager_t* mm, int pid, int addr,
                    int* out_value)
{
    if (mm == NULL || out_value == NULL) {
        return MM_ERR_ARG;
    }
    mm_page_table_t* pt = NULL;
    int frame = -1, offset = 0;
    mm_status_t st = resolve(mm, pid, addr, &pt, &frame, &offset);
    if (st != MM_OK) {
        return st;
    }
    *out_value = mm->data[frame][offset];
    return MM_OK;
}

mm_status_t mm_write(jvk_memory_manager_t* mm, int pid, int addr, int value)
{
    if (mm == NULL) {
        return MM_ERR_ARG;
    }
    mm_page_table_t* pt = NULL;
    int frame = -1, offset = 0;
    mm_status_t st = resolve(mm, pid, addr, &pt, &frame, &offset);
    if (st != MM_OK) {
        return st;
    }
    mm->data[frame][offset] = value;
    mm_frame_t* f           = ft_at(&mm->ft, frame);
    f->access_seq           = ++mm->seq;
    pt_entry(pt, addr / JVK_MM_PAGE_WORDS)->dirty = 1;
    return MM_OK;
}
