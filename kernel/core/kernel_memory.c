/*
 * kernel/core/kernel_memory.c
 *
 * JARVIS OS — kernel-layer glue for the memory subsystem.
 *
 * Boot config keys (all optional, section "memory"):
 *   totalPages     physical frames        (default 256)
 *   allocator      first_fit|best_fit|worst_fit   (default first_fit)
 *   replacement    fifo|lru|clock                 (default clock)
 *   swapEnabled                                   (default true)
 *   swapSlots                                     (default 128)
 */

#include <stdio.h>
#include <string.h>

#include "kernel_memory.h"

#define KMEM_DEFAULT_FRAMES   256
#define KMEM_DEFAULT_SWAPSLOTS 128

/* ---- boot -------------------------------------------------------------- */

static void parse_memory_config(const cJSON* root, mm_config_t* cfg)
{
    const cJSON* mem = cJSON_GetObjectItemCaseSensitive(root, "memory");
    if (!cJSON_IsObject(mem)) {
        return;
    }
    cJSON* frames = cJSON_GetObjectItemCaseSensitive(mem, "totalPages");
    if (cJSON_IsNumber(frames) && frames->valuedouble >= 1) {
        cfg->total_frames = (int)frames->valuedouble;
    }
    cJSON* alloc = cJSON_GetObjectItemCaseSensitive(mem, "allocator");
    if (cJSON_IsString(alloc)) {
        mm_alloc_parse(alloc->valuestring, &cfg->allocator);
    }
    cJSON* repl = cJSON_GetObjectItemCaseSensitive(mem, "replacement");
    if (cJSON_IsString(repl)) {
        mm_replace_parse(repl->valuestring, &cfg->replacement);
    }
    cJSON* sw = cJSON_GetObjectItemCaseSensitive(mem, "swapEnabled");
    if (cJSON_IsBool(sw)) {
        cfg->swap_enabled = cJSON_IsTrue(sw);
    }
    cJSON* slots = cJSON_GetObjectItemCaseSensitive(mem, "swapSlots");
    if (cJSON_IsNumber(slots) && slots->valuedouble >= 0) {
        cfg->swap_slots = (int)slots->valuedouble;
    }
}

int kmem_boot(jvk_memory_manager_t* mm, const cJSON* config_root)
{
    mm_config_t cfg = {
        .total_frames = KMEM_DEFAULT_FRAMES,
        .allocator    = MM_ALLOC_FIRST_FIT,
        .replacement  = MM_REPLACE_CLOCK,
        .swap_enabled = 1,
        .swap_slots   = KMEM_DEFAULT_SWAPSLOTS,
    };
    if (config_root != NULL) {
        parse_memory_config(config_root, &cfg);
    }
    return mm_init(mm, &cfg);
}

/* ---- command handlers --------------------------------------------------- */

static int handle_mem_alloc(jvk_memory_manager_t* mm, const cJSON* req,
                            cJSON* result)
{
    const cJSON* pid_item   = cJSON_GetObjectItemCaseSensitive(req, "pid");
    const cJSON* pages_item = cJSON_GetObjectItemCaseSensitive(req, "pages");
    int pid   = cJSON_IsNumber(pid_item) ? (int)pid_item->valuedouble : -1;
    int pages = cJSON_IsNumber(pages_item) ? (int)pages_item->valuedouble : 1;

    mm_alloc_result_t out;
    mm_status_t st = mm_alloc(mm, pid, pages, &out);
    if (st != MM_OK) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", mm_status_name(st));
        return 1;
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "pid", pid);
    cJSON_AddNumberToObject(result, "base_vpage", out.base_vpage);
    cJSON_AddNumberToObject(result, "pages", out.count);
    cJSON_AddBoolToObject(result, "contiguous", out.contiguous);
    cJSON* arr = cJSON_CreateArray();
    for (int k = 0; k < out.count; k++) {
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(out.frames[k]));
    }
    cJSON_AddItemToObject(result, "frames", arr);
    return 1;
}

static int handle_mem_read(jvk_memory_manager_t* mm, const cJSON* req,
                           cJSON* result)
{
    const cJSON* pid_item  = cJSON_GetObjectItemCaseSensitive(req, "pid");
    const cJSON* addr_item = cJSON_GetObjectItemCaseSensitive(req, "addr");
    int pid  = cJSON_IsNumber(pid_item) ? (int)pid_item->valuedouble : -1;
    int addr = cJSON_IsNumber(addr_item) ? (int)addr_item->valuedouble : -1;

    int value = 0;
    mm_status_t st = mm_read(mm, pid, addr, &value);
    if (st != MM_OK) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", mm_status_name(st));
        return 1;
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "pid", pid);
    cJSON_AddNumberToObject(result, "addr", addr);
    cJSON_AddNumberToObject(result, "vpage", addr / JVK_MM_PAGE_WORDS);
    cJSON_AddNumberToObject(result, "offset", addr % JVK_MM_PAGE_WORDS);
    cJSON_AddNumberToObject(result, "value", value);
    return 1;
}

int kmem_handle(jvk_memory_manager_t* mm, const char* action,
                const cJSON* req, cJSON* result)
{
    if (strcmp(action, "mem_alloc") == 0) {
        return handle_mem_alloc(mm, req, result);
    }
    if (strcmp(action, "mem_free") == 0) {
        const cJSON* pid_item =
            cJSON_GetObjectItemCaseSensitive(req, "pid");
        int pid = cJSON_IsNumber(pid_item) ? (int)pid_item->valuedouble : -1;
        int freed = mm_free_pid(mm, pid);
        if (freed < 0) {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", mm_status_name(MM_ERR_NO_PID));
            return 1;
        }
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddNumberToObject(result, "pid", pid);
        cJSON_AddNumberToObject(result, "pages_freed", freed);
        return 1;
    }
    if (strcmp(action, "mem_write") == 0) {
        const cJSON* pid_item   = cJSON_GetObjectItemCaseSensitive(req, "pid");
        const cJSON* addr_item  = cJSON_GetObjectItemCaseSensitive(req, "addr");
        const cJSON* value_item = cJSON_GetObjectItemCaseSensitive(req, "value");
        int pid   = cJSON_IsNumber(pid_item) ? (int)pid_item->valuedouble : -1;
        int addr  = cJSON_IsNumber(addr_item) ? (int)addr_item->valuedouble : -1;
        int value = cJSON_IsNumber(value_item) ? (int)value_item->valuedouble : 0;
        mm_status_t st = mm_write(mm, pid, addr, value);
        cJSON_AddBoolToObject(result, "ok", st == MM_OK ? 1 : 0);
        if (st != MM_OK) {
            cJSON_AddStringToObject(result, "error", mm_status_name(st));
        } else {
            cJSON_AddNumberToObject(result, "pid", pid);
            cJSON_AddNumberToObject(result, "addr", addr);
            cJSON_AddNumberToObject(result, "vpage",
                                    addr / JVK_MM_PAGE_WORDS);
            cJSON_AddNumberToObject(result, "offset",
                                    addr % JVK_MM_PAGE_WORDS);
        }
        return 1;
    }
    if (strcmp(action, "mem_read") == 0) {
        return handle_mem_read(mm, req, result);
    }
    if (strcmp(action, "mem_config") == 0) {
        const cJSON* alloc = cJSON_GetObjectItemCaseSensitive(req, "allocator");
        if (cJSON_IsString(alloc) && !mm_set_allocator(mm, alloc->valuestring)) {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "unknown allocator");
            return 1;
        }
        const cJSON* repl = cJSON_GetObjectItemCaseSensitive(req, "replacement");
        if (cJSON_IsString(repl) && !mm_set_replacement(mm, repl->valuestring)) {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "unknown replacement policy");
            return 1;
        }
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddStringToObject(result, "allocator",
                                mm_alloc_name(mm->cfg.allocator));
        cJSON_AddStringToObject(result, "replacement",
                                mm_replace_name(mm->cfg.replacement));
        cJSON_AddNumberToObject(result, "total_frames", mm->cfg.total_frames);
        return 1;
    }
    return 0;
}

/* ---- snapshot ------------------------------------------------------------ */

void kmem_snapshot(const jvk_memory_manager_t* mm, cJSON* root)
{
    cJSON* mem = cJSON_CreateObject();

    cJSON* cfg = cJSON_CreateObject();
    cJSON_AddNumberToObject(cfg, "total_frames", mm->cfg.total_frames);
    cJSON_AddNumberToObject(cfg, "page_words", JVK_MM_PAGE_WORDS);
    cJSON_AddStringToObject(cfg, "allocator", mm_alloc_name(mm->cfg.allocator));
    cJSON_AddStringToObject(cfg, "replacement",
                            mm_replace_name(mm->cfg.replacement));
    cJSON_AddBoolToObject(cfg, "swap_enabled", mm->cfg.swap_enabled);
    cJSON_AddNumberToObject(cfg, "swap_slots", mm->cfg.swap_slots);
    cJSON_AddItemToObject(mem, "config", cfg);

    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "frames_used", mm->ft.total - ft_free_count(&mm->ft));
    cJSON_AddNumberToObject(stats, "frames_free", ft_free_count(&mm->ft));
    cJSON_AddNumberToObject(stats, "pages_mapped", mm->pages_mapped);
    cJSON_AddNumberToObject(stats, "allocs", mm->allocs);
    cJSON_AddNumberToObject(stats, "frees", mm->frees);
    cJSON_AddNumberToObject(stats, "page_faults", mm->page_faults);
    cJSON_AddNumberToObject(stats, "swap_ins", mm->swap_ins);
    cJSON_AddNumberToObject(stats, "swap_outs", mm->swap_outs);
    cJSON_AddNumberToObject(stats, "segfaults", mm->segfaults);
    cJSON_AddNumberToObject(stats, "fragmented_allocs", mm->fragmented_allocs);
    cJSON_AddItemToObject(mem, "stats", stats);

    cJSON_AddNumberToObject(mem, "swap_used", swap_used(&mm->swap));

    cJSON* frame_map = cJSON_CreateArray();
    for (int i = 0; i < mm->ft.total; i++) {
        int owner = mm->ft.frames[i].state == MM_FRAME_USED
                        ? mm->ft.frames[i].owner_pid : 0;
        cJSON_AddItemToArray(frame_map, cJSON_CreateNumber(owner));
    }
    cJSON_AddItemToObject(mem, "frame_map", frame_map);

    cJSON* tables = cJSON_CreateArray();
    for (int i = 0; i < JVK_MM_MAX_PROCS; i++) {
        if (mm->pids[i] == 0) {
            continue;
        }
        cJSON* t = cJSON_CreateObject();
        cJSON_AddNumberToObject(t, "pid", mm->pids[i]);
        cJSON* pages = cJSON_CreateArray();
        for (int v = 0; v < mm->tables[i].used_count; v++) {
            const mm_pte_t* e = &mm->tables[i].entries[v];
            cJSON* p = cJSON_CreateObject();
            cJSON_AddNumberToObject(p, "vpage", v);
            cJSON_AddNumberToObject(p, "frame", e->frame);
            cJSON_AddBoolToObject(p, "present", e->present);
            cJSON_AddBoolToObject(p, "referenced", e->referenced != 0);
            cJSON_AddBoolToObject(p, "dirty", e->dirty != 0);
            cJSON_AddItemToArray(pages, p);
        }
        cJSON_AddItemToObject(t, "pages", pages);
        cJSON_AddItemToArray(tables, t);
    }
    cJSON_AddItemToObject(mem, "page_tables", tables);

    cJSON_AddItemToObject(root, "memory", mem);
}
