#include <stddef.h>
/*
 * kernel/tests/test_memory.c
 *
 * JARVIS OS — memory manager smoke test (M3b).
 *
 * Exercises allocation strategies (first/best/worst fit), scattered
 * fallback under fragmentation, replacement policies (FIFO/LRU/Clock),
 * swap-out/swap-in data preservation, segfault detection and teardown.
 * Kernel events are captured through the observer callback.
 * Run from the kernel/ directory:
 *
 *     mingw32-make test
 */

#include <stdio.h>
#include <string.h>

#include "memory/memory_manager.h"

#define MAX_EVENTS 128

static int  g_failures = 0;
static char g_events[MAX_EVENTS][96];
static int  g_event_count = 0;

static void capture(void* user, const char* message)
{
    (void)user;
    if (g_event_count < MAX_EVENTS) {
        snprintf(g_events[g_event_count], sizeof(g_events[0]), "%s", message);
        g_event_count++;
    }
}

static void reset_events(void)
{
    g_event_count = 0;
}

static int has_event(const char* prefix)
{
    size_t len = strlen(prefix);
    for (int i = 0; i < g_event_count; i++) {
        if (strncmp(g_events[i], prefix, len) == 0) {
            return 1;
        }
    }
    return 0;
}

static void expect(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

static mm_pte_t* pte_of(jvk_memory_manager_t* mm, int pid, int vpage)
{
    for (int i = 0; i < JVK_MM_MAX_PROCS; i++) {
        if (mm->pids[i] == pid) {
            return pt_entry(&mm->tables[i], vpage);
        }
    }
    return NULL;
}

static void test_read_write_roundtrip(void)
{
    jvk_memory_manager_t mm;
    mm_config_t cfg = {16, MM_ALLOC_FIRST_FIT, MM_REPLACE_CLOCK, 1, 8};
    expect(mm_init(&mm, &cfg) == 1, "init 16-frame manager");

    mm_set_observer(&mm, capture, NULL);

    mm_alloc_result_t r;
    expect(mm_alloc(&mm, 1, 2, &r) == MM_OK, "alloc 2 pages for pid 1");
    expect(r.base_vpage == 0 && r.count == 2, "base vpage is 0");
    expect(r.contiguous == 1 && r.frames[0] == 0 && r.frames[1] == 1,
           "first_fit places contiguous low frames");

    expect(mm_write(&mm, 1, 5, 42) == MM_OK, "write vaddr 5");
    expect(mm_write(&mm, 1, 19, 77) == MM_OK, "write vaddr 19 (second page)");
    int v = 0;
    expect(mm_read(&mm, 1, 5, &v) == MM_OK && v == 42, "read back 42");
    expect(mm_read(&mm, 1, 19, &v) == MM_OK && v == 77, "read back 77");

    mm_pte_t* e0 = pte_of(&mm, 1, 0);
    mm_pte_t* e1 = pte_of(&mm, 1, 1);
    expect(e0 != NULL && e0->present && e0->dirty, "page 0 resident+dirty");
    expect(e1 != NULL && e1->present && e1->dirty, "page 1 resident+dirty");
    expect(has_event("MEMORY_ALLOCATED pid=1 pages=2"),
           "allocation event emitted");

    /* fresh pages read as zero */
    expect(mm_read(&mm, 1, 30, &v) == MM_OK && v == 0, "fresh page reads zero");

    expect(mm.segfaults == 0, "no segfaults yet");
    mm_shutdown(&mm);
}

static void test_protection_errors(void)
{
    jvk_memory_manager_t mm;
    mm_config_t cfg = {8, MM_ALLOC_FIRST_FIT, MM_REPLACE_CLOCK, 1, 4};
    mm_init(&mm, &cfg);
    mm_set_observer(&mm, capture, NULL);

    int v = 0;
    expect(mm_read(&mm, 99, 0, &v) == MM_ERR_NO_PID,
           "read for unknown pid rejected");
    expect(mm_alloc(&mm, 1, 1, &(mm_alloc_result_t){0}) == MM_OK,
           "alloc one page");
    expect(mm_read(&mm, 1, JVK_MM_PAGE_WORDS * 5, &v) == MM_ERR_SEGV,
           "unmapped vpage raises SEGV");
    expect(mm_read(&mm, 1, -1, &v) == MM_ERR_SEGV, "negative addr raises SEGV");
    expect(mm_write(&mm, 1, 999999, 1) == MM_ERR_SEGV,
           "far write raises SEGV");
    expect(mm.segfaults == 3 && has_event("SEGV pid=1"),
           "segfaults counted and logged");

    expect(mm_alloc(&mm, 1, JVK_MM_PT_ENTRIES, &(mm_alloc_result_t){0})
               == MM_ERR_ARG, "over-sized alloc rejected");
    mm_shutdown(&mm);
}

/* Fragment a tiny heap so that fit strategies must disagree:
   A(4 pages @0..3), B(2 @4..5), C(2 @6..7); freeing A and C leaves two
   non-adjacent holes (len4 @0, len2 @6). A request of 2 pages goes low
   for first/worst fit and exact-fits high for best fit. */
static void test_fit_strategies_disagree(void)
{
    static const struct {
        mm_alloc_strategy_t strat;
        int f0;
    } cases[] = {
        {MM_ALLOC_FIRST_FIT, 0},
        {MM_ALLOC_WORST_FIT, 0},
        {MM_ALLOC_BEST_FIT,  6},
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        jvk_memory_manager_t mm;
        mm_config_t cfg = {8, cases[c].strat, MM_REPLACE_CLOCK, 0, 0};
        mm_init(&mm, &cfg);

        mm_alloc_result_t ra, rb, rc;
        expect(mm_alloc(&mm, 1, 4, &ra) == MM_OK, "A fills 0-3");
        expect(ra.frames[0] == 0 && ra.frames[3] == 3, "A placement");
        expect(mm_alloc(&mm, 2, 2, &rb) == MM_OK, "B fills 4-5");
        expect(mm_alloc(&mm, 3, 2, &rc) == MM_OK, "C fills 6-7");
        expect(mm_free_pid(&mm, 1) == 4, "free A (4 pages)");
        expect(mm_free_pid(&mm, 3) == 2, "free C (2 pages)");

        mm_alloc_result_t re;
        expect(mm_alloc(&mm, 5, 2, &re) == MM_OK, "alloc E into holes");
        if (re.frames[0] != cases[c].f0) {
            printf("  (%s placed at %d not %d)\n",
                   mm_alloc_name(cases[c].strat), re.frames[0], cases[c].f0);
        }
        expect(re.frames[0] == cases[c].f0, mm_alloc_name(cases[c].strat));
        mm_shutdown(&mm);
    }
}

static void test_scattered_fallback(void)
{
    jvk_memory_manager_t mm;
    mm_config_t cfg = {8, MM_ALLOC_FIRST_FIT, MM_REPLACE_CLOCK, 0, 0};
    mm_init(&mm, &cfg);
    mm_set_observer(&mm, capture, NULL);

    mm_alloc_result_t r;
    /* fill every frame, then release alternating owners so the free
       list becomes {1,3,5,7} — no two adjacent */
    for (int pid = 1; pid <= 8; pid++) {
        expect(mm_alloc(&mm, pid, 1, &r) == MM_OK, "fill one frame");
    }
    for (int pid = 2; pid <= 8; pid += 2) {
        expect(mm_free_pid(&mm, pid) == 1, "release alternate frames");
    }

    reset_events();
    expect(mm_alloc(&mm, 9, 4, &r) == MM_OK, "scattered alloc succeeds");
    expect(r.contiguous == 0, "placement reported fragmented");
    expect(r.frames[0] == 1 && r.frames[1] == 3 && r.frames[2] == 5 &&
               r.frames[3] == 7,
           "scattered frames collected in address order");
    expect(mm.fragmented_allocs == 1 && has_event("EXTERNAL_FRAGMENTATION"),
           "fragmentation counted and logged");
    mm_shutdown(&mm);
}

static void test_swap_preserves_data(void)
{
    jvk_memory_manager_t mm;
    mm_config_t cfg = {4, MM_ALLOC_FIRST_FIT, MM_REPLACE_CLOCK, 1, 8};
    mm_init(&mm, &cfg);
    mm_set_observer(&mm, capture, NULL);

    mm_alloc_result_t r;
    expect(mm_alloc(&mm, 1, 2, &r) == MM_OK, "P1 owns frames 0-1");
    expect(mm_alloc(&mm, 2, 2, &r) == MM_OK, "P2 owns frames 2-3");
    expect(mm_write(&mm, 1, 0, 777) == MM_OK, "store 777 in P1 page 0");

    reset_events();
    expect(mm_alloc(&mm, 3, 1, &r) == MM_OK,
           "P3 allocation forces an eviction");
    expect(mm.swap_outs == 1 && has_event("SWAP_OUT"),
           "victim swapped out");
    expect(swap_used(&mm.swap) == 1, "swap slot occupied");

    int v = 0;
    expect(mm_read(&mm, 1, 0, &v) == MM_OK, "faulting read succeeds");
    expect(v == 777, "value survived the swap round-trip");
    expect(mm.page_faults >= 1 && has_event("PAGE_FAULT"),
           "access raised PAGE_FAULT");
    expect(mm.swap_ins == 1 && has_event("SWAP_IN"), "page swapped back in");

    /* release the pressure, fault in cleanly: swap must drain to zero */
    mm_free_pid(&mm, 3);
    mm_free_pid(&mm, 2);
    int w = 0;
    expect(mm_read(&mm, 1, JVK_MM_PAGE_WORDS, &w) == MM_OK,
           "second page faults back in");
    expect(w == 0, "clean page restored as zeros");
    expect(swap_used(&mm.swap) == 0, "swap drained to zero slots");
    mm_shutdown(&mm);
}

static void test_fifo_vs_lru_victim(void)
{
    /* Both managers: P1 4 pages, P2 4 pages, then touch P1 page 0.
       A 1-page allocation must evict:
         FIFO -> P1 page 0 (oldest load, despite recent touch)
         LRU  -> P1 page 1 (least recently accessed overall)   */
    static const struct {
        mm_replace_policy_t policy;
        int                 victim_frame;
        int                 victim_vpage;
    } cases[] = {
        {MM_REPLACE_FIFO, 0, 0},
        {MM_REPLACE_LRU,  1, 1},
    };

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        jvk_memory_manager_t mm;
        mm_config_t cfg = {8, MM_ALLOC_FIRST_FIT, cases[c].policy, 1, 8};
        mm_init(&mm, &cfg);

        mm_alloc_result_t r;
        mm_alloc(&mm, 1, 4, &r);
        mm_alloc(&mm, 2, 4, &r);
        int v = 0;
        expect(mm_read(&mm, 1, 0, &v) == MM_OK, "touch P1 page 0");

        expect(mm_alloc(&mm, 3, 1, &r) == MM_OK, "pressure alloc");
        expect(r.frames[0] == cases[c].victim_frame,
               mm_replace_name(cases[c].policy));
        mm_pte_t* victim_pte =
            pte_of(&mm, 1, cases[c].victim_vpage);
        expect(victim_pte != NULL && !victim_pte->present &&
                   victim_pte->swap_slot >= 0,
               "victim page marked swapped");
        mm_shutdown(&mm);
    }

    expect(strcmp(mm_replace_name(MM_REPLACE_CLOCK), "clock") == 0,
           "policy names stable");
}

static void test_oom_without_swap(void)
{
    jvk_memory_manager_t mm;
    mm_config_t cfg = {4, MM_ALLOC_FIRST_FIT, MM_REPLACE_CLOCK, 0, 0};
    mm_init(&mm, &cfg);

    mm_alloc_result_t r;
    expect(mm_alloc(&mm, 1, 4, &r) == MM_OK, "fill physical memory");
    /* no free frames + swap off: OOM wins even for the owning pid */
    expect(mm_alloc(&mm, 2, 1, &r) == MM_ERR_OOM, "OOM for new pid");
    expect(mm_alloc(&mm, 1, 1, &r) == MM_ERR_OOM, "OOM without swap");
    mm_shutdown(&mm);

    /* the per-process address-space cap is a separate limit */
    jvk_memory_manager_t big;
    mm_config_t cfg2 = {128, MM_ALLOC_FIRST_FIT, MM_REPLACE_CLOCK, 0, 0};
    mm_init(&big, &cfg2);
    expect(mm_alloc(&big, 1, JVK_MM_PT_ENTRIES, &r) == MM_OK,
           "fill whole address space (64 pages)");
    expect(mm_alloc(&big, 1, 1, &r) == MM_ERR_ARG,
           "address-space cap raises ARG");
    mm_shutdown(&big);
}

static void test_runtime_strategy_switch(void)
{
    jvk_memory_manager_t mm;
    mm_config_t cfg = {8, MM_ALLOC_FIRST_FIT, MM_REPLACE_FIFO, 1, 4};
    mm_init(&mm, &cfg);

    expect(mm_set_allocator(&mm, "worst_fit") == 1, "switch allocator");
    expect(strcmp(mm_alloc_name(mm.cfg.allocator), "worst_fit") == 0,
           "allocator switched");
    expect(mm_set_replacement(&mm, "lru") == 1, "switch replacement");
    expect(strcmp(mm_replace_name(mm.cfg.replacement), "lru") == 0,
           "replacement switched");
    expect(mm_set_allocator(&mm, "turbo") == 0, "bad allocator rejected");
    expect(mm_set_replacement(&mm, "mru") == 0, "bad policy rejected");

    mm_free_pid(&mm, 42); /* unknown pid: silent no-op path */
    expect(mm.frees == 0, "free unknown pid does not count");
    mm_shutdown(&mm);
}

int main(void)
{
    test_read_write_roundtrip();
    test_protection_errors();
    test_fit_strategies_disagree();
    test_scattered_fallback();
    test_swap_preserves_data();
    test_fifo_vs_lru_victim();
    test_oom_without_swap();
    test_runtime_strategy_switch();

    if (g_failures == 0) {
        printf("PASS: memory manager smoke test\n");
        return 0;
    }
    printf("memory manager smoke test FAILED (%d assertion(s))\n",
           g_failures);
    return 1;
}
