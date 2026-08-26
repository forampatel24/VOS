/*
 * kernel/memory/alloc_strategy.h
 *
 * JARVIS OS — frame placement strategies.
 *
 * The classic contiguous-fit algorithms operate on runs of free frames.
 * When physical memory is too fragmented to satisfy a request with one
 * run, the manager falls back to scattered placement so allocation still
 * succeeds — making external fragmentation observable instead of fatal.
 */

#ifndef JARVIS_ALLOC_STRATEGY_H
#define JARVIS_ALLOC_STRATEGY_H

#include "frame_table.h"
#include "mm_types.h"

/* Start index of the best contiguous free run of at least `need`
   frames under `strategy`, or -1 when no single run is big enough. */
int as_find_contiguous(const mm_frame_table_t* ft,
                       mm_alloc_strategy_t strategy,
                       int need);

/* Collect up to `need` free frames anywhere (lowest index first).
   Returns how many were found. */
int as_collect_scattered(const mm_frame_table_t* ft,
                         int need, int* out_frames, int out_cap);

#endif /* JARVIS_ALLOC_STRATEGY_H */
