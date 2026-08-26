/*
 * kernel/memory/alloc_strategy.c
 *
 * JARVIS OS — frame placement strategies (first / best / worst fit).
 *
 * first_fit : lowest run that is big enough.
 * best_fit  : smallest run that is still big enough (least leftover).
 * worst_fit : largest run available (largest leftover for later splits).
 */

#include "alloc_strategy.h"

static int fits_strategy(int run, int best_run, mm_alloc_strategy_t strategy,
                         int have_candidate)
{
    switch (strategy) {
    case MM_ALLOC_FIRST_FIT:
        return !have_candidate; /* first one wins; caller stops scanning */
    case MM_ALLOC_BEST_FIT:
        return !have_candidate || run < best_run;
    case MM_ALLOC_WORST_FIT:
        return !have_candidate || run > best_run;
    default:
        return 0;
    }
}

int as_find_contiguous(const mm_frame_table_t* ft,
                       mm_alloc_strategy_t strategy,
                       int need)
{
    if (ft == NULL || need <= 0) {
        return -1;
    }

    int best_start = -1;
    int best_run   = 0;

    for (int i = 0; i < ft->total; i++) {
        int run = ft_run_length(ft, i);
        if (run <= 0) {
            continue;
        }
        if (run >= need && fits_strategy(run, best_run, strategy, best_start >= 0)) {
            best_start = i;
            best_run   = run;
            if (strategy == MM_ALLOC_FIRST_FIT) {
                break;
            }
        }
        i += run - 1; /* jump past this run */
    }

    return best_start;
}

int as_collect_scattered(const mm_frame_table_t* ft,
                         int need, int* out_frames, int out_cap)
{
    if (ft == NULL || need <= 0 || out_frames == NULL || out_cap <= 0) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < ft->total && count < need && count < out_cap; i++) {
        if (ft->frames[i].state == MM_FRAME_FREE) {
            out_frames[count++] = i;
        }
    }
    return count;
}
