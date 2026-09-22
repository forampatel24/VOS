#include <stddef.h>
/*
 * kernel/memory/replace_policy.c
 *
 * JARVIS OS — page replacement strategies (FIFO / LRU / Clock).
 *
 * FIFO  : evict the page loaded longest ago (min loaded_seq).
 * LRU   : evict the page touched longest ago (min access_seq).
 * Clock : sweep the circular frame list; give referenced frames a
 *         second chance (clear ref bit, advance), evict the first
 *         unreferenced frame found.
 */

#include "replace_policy.h"

static int pick_min_stamp(const mm_frame_table_t* ft, int by_loaded)
{
    int victim      = -1;
    unsigned long best = 0;
    for (int i = 0; i < ft->total; i++) {
        const mm_frame_t* f = &ft->frames[i];
        if (f->state != MM_FRAME_USED) {
            continue;
        }
        unsigned long stamp = by_loaded ? f->loaded_seq : f->access_seq;
        if (victim < 0 || stamp < best) {
            victim = i;
            best   = stamp;
        }
    }
    return victim;
}

static int pick_clock(const mm_frame_table_t* ft, int* hand)
{
    if (ft->total <= 0) {
        return -1;
    }
    int cursor = (*hand < 0 || *hand >= ft->total) ? 0 : *hand;

    /* One full pass clears reference bits; the second must find a victim
       because at least one used frame exists. */
    for (int step = 0; step < 2 * ft->total; step++) {
        mm_frame_t* f = &((mm_frame_table_t*)ft)->frames[cursor];
        if (f->state == MM_FRAME_USED) {
            if (f->ref_bit) {
                f->ref_bit = 0; /* second chance */
            } else {
                *hand = (cursor + 1) % ft->total;
                return cursor;
            }
        }
        cursor = (cursor + 1) % ft->total;
    }
    return -1;
}

int rp_pick_victim(mm_frame_table_t* ft,
                   mm_replace_policy_t policy,
                   int* hand)
{
    if (ft == NULL) {
        return -1;
    }
    switch (policy) {
    case MM_REPLACE_FIFO:  return pick_min_stamp(ft, 1);
    case MM_REPLACE_LRU:   return pick_min_stamp(ft, 0);
    case MM_REPLACE_CLOCK: return pick_clock(ft, hand);
    default:               return -1;
    }
}
