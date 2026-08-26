/*
 * kernel/memory/mm_types.c
 *
 * JARVIS OS — names and parsers for the shared memory types.
 * Strategy selection is data-driven: config strings map to enums here
 * and nowhere else.
 */

#include <string.h>

#include "mm_types.h"

const char* mm_alloc_name(mm_alloc_strategy_t s)
{
    switch (s) {
    case MM_ALLOC_FIRST_FIT: return "first_fit";
    case MM_ALLOC_BEST_FIT:  return "best_fit";
    case MM_ALLOC_WORST_FIT: return "worst_fit";
    default:                 return "unknown";
    }
}

int mm_alloc_parse(const char* name, mm_alloc_strategy_t* out)
{
    if (name == NULL || out == NULL) {
        return 0;
    }
    if (strcmp(name, "first_fit") == 0) {
        *out = MM_ALLOC_FIRST_FIT;
        return 1;
    }
    if (strcmp(name, "best_fit") == 0) {
        *out = MM_ALLOC_BEST_FIT;
        return 1;
    }
    if (strcmp(name, "worst_fit") == 0) {
        *out = MM_ALLOC_WORST_FIT;
        return 1;
    }
    return 0;
}

const char* mm_replace_name(mm_replace_policy_t p)
{
    switch (p) {
    case MM_REPLACE_FIFO:   return "fifo";
    case MM_REPLACE_LRU:    return "lru";
    case MM_REPLACE_CLOCK:  return "clock";
    default:                return "unknown";
    }
}

int mm_replace_parse(const char* name, mm_replace_policy_t* out)
{
    if (name == NULL || out == NULL) {
        return 0;
    }
    if (strcmp(name, "fifo") == 0) {
        *out = MM_REPLACE_FIFO;
        return 1;
    }
    if (strcmp(name, "lru") == 0) {
        *out = MM_REPLACE_LRU;
        return 1;
    }
    if (strcmp(name, "clock") == 0) {
        *out = MM_REPLACE_CLOCK;
        return 1;
    }
    return 0;
}

const char* mm_status_name(mm_status_t s)
{
    switch (s) {
    case MM_OK:          return "ok";
    case MM_ERR_ARG:     return "bad argument";
    case MM_ERR_NO_PID:  return "no address space for pid";
    case MM_ERR_OOM:     return "out of memory";
    case MM_ERR_SEGV:    return "segmentation fault";
    default:             return "unknown error";
    }
}
