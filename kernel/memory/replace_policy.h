/*
 * kernel/memory/replace_policy.h
 *
 * JARVIS OS — page replacement strategies.
 *
 * Victim selection is interchangeable at runtime (Strategy Pattern).
 * All three policies operate on the same global frame table and may
 * evict any resident page (global replacement). The manager owns the
 * clock hand; this module only advances it.
 */

#ifndef JARVIS_REPLACE_POLICY_H
#define JARVIS_REPLACE_POLICY_H

#include "frame_table.h"
#include "mm_types.h"

/* Pick the next victim frame. Returns the frame index or -1 when the
   table has no used frames. `hand` is the clock cursor (in/out); other
   policies ignore it. The clock policy clears reference bits while
   scanning, so it needs mutable access. */
int rp_pick_victim(mm_frame_table_t* ft,
                   mm_replace_policy_t policy,
                   int* hand);

#endif /* JARVIS_REPLACE_POLICY_H */
