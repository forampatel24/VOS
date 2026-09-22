#include <stddef.h>
/*
 * kernel/tests/test_interrupts.c
 *
 * JARVIS OS — interrupt controller & error manager smoke test (M4).
 *
 * Verifies priority queue ordering, FIFO within same priority, ISR
 * dispatch, panic flag, and error categorization.
 * Run from kernel/:  mingw32-make test
 */

#include <stdio.h>
#include <string.h>

#include "interrupts/error_manager.h"
#include "interrupts/ic.h"
#include "interrupts/isr.h"

static int g_failures = 0;

static void expect(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

static int g_isr_count = 0;
static void counting_isr(jvk_ic_t* ic, const jvk_irq_entry_t* e, void* user)
{
    (void)ic;
    (void)e;
    (void)user;
    g_isr_count++;
}

int main(void)
{
    /* ---- priority ordering ------------------------------------------- */
    jvk_ic_t ic;
    ic_init(&ic);
    expect(ic_pending(&ic) == 0, "empty at init");
    expect(ic_enqueue(&ic, JVK_IRQ_TIMER, "timer", "quantum expired", 10) == 1, "enqueue timer");
    expect(ic_enqueue(&ic, JVK_IRQ_PAGE_FAULT, "memory", "pid=1 vpage=2", 11) == 1, "enqueue page fault");
    expect(ic_enqueue(&ic, JVK_IRQ_SOFTWARE, "test", "soft", 12) == 1, "enqueue software");
    expect(ic_pending(&ic) == 3, "three pending");

    jvk_irq_entry_t out;
    expect(ic_dequeue_next(&ic, &out) == 1 && out.irq == JVK_IRQ_PAGE_FAULT, "page fault (prio 1) dequeued first");
    expect(ic_dequeue_next(&ic, &out) == 1 && out.irq == JVK_IRQ_TIMER, "timer (prio 4) second");
    expect(ic_dequeue_next(&ic, &out) == 1 && out.irq == JVK_IRQ_SOFTWARE, "software (prio 5) last");
    expect(ic_pending(&ic) == 0, "queue empty after drain");
    expect(ic.handled == 3, "handled count 3");

    /* ---- FIFO within same priority ----------------------------------- */
    ic_init(&ic);
    ic_enqueue(&ic, JVK_IRQ_TIMER, "timer", "a", 5);
    ic_enqueue(&ic, JVK_IRQ_TIMER, "timer", "b", 6);
    ic_enqueue(&ic, JVK_IRQ_TIMER, "timer", "c", 7);
    ic_dequeue_next(&ic, &out);
    expect(strcmp(out.detail, "a") == 0, "FIFO: first timer a");
    ic_dequeue_next(&ic, &out);
    expect(strcmp(out.detail, "b") == 0, "FIFO: second timer b");

    /* ---- shutdown highest priority ----------------------------------- */
    ic_init(&ic);
    ic_enqueue(&ic, JVK_IRQ_TIMER, "timer", "t", 20);
    ic_enqueue(&ic, JVK_IRQ_SHUTDOWN, "test", "panic test", 21);
    ic_enqueue(&ic, JVK_IRQ_DISK, "disk", "d", 22);
    ic_dequeue_next(&ic, &out);
    expect(out.irq == JVK_IRQ_SHUTDOWN, "shutdown (prio 0) beats all");

    /* ---- queue cap & drop count -------------------------------------- */
    ic_init(&ic);
    for (int i = 0; i < JVK_IC_CAP; i++) {
        expect(ic_enqueue(&ic, JVK_IRQ_SOFTWARE, "fill", "x", (unsigned long)i) == 1, "fill queue");
    }
    expect(ic_enqueue(&ic, JVK_IRQ_SOFTWARE, "fill", "overflow", 99) == 0, "overflow rejected");
    expect(ic.dropped == 1, "dropped count 1");

    /* ---- ISR dispatch ------------------------------------------------- */
    jvk_isr_registry_t reg;
    isr_init(&reg);
    isr_register_defaults(&reg);
    g_isr_count = 0;
    isr_register(&reg, JVK_IRQ_TIMER, counting_isr, NULL);
    ic_init(&ic);
    ic_enqueue(&ic, JVK_IRQ_TIMER, "timer", "isr test", 30);
    ic_dequeue_next(&ic, &out);
    isr_handle(&reg, &ic, &out);
    expect(g_isr_count == 1, "custom ISR called");

    /* ---- panic flag --------------------------------------------------- */
    ic_init(&ic);
    expect(ic.panic == 0, "no panic at init");
    ic_set_panic(&ic, "test panic");
    expect(ic.panic == 1, "panic set");
    expect(strcmp(ic.panic_reason, "test panic") == 0, "panic reason");
    ic_clear_panic(&ic);
    expect(ic.panic == 0, "panic cleared");

    /* ---- error manager ------------------------------------------------ */
    jvk_error_manager_t em;
    em_init(&em);
    expect(em.counts[JVK_ERR_MEMORY] == 0, "no memory errors at init");
    ic_init(&ic);
    em_record(&em, JVK_ERR_MEMORY, "SEGV pid=1 addr=999", &ic);
    expect(em.counts[JVK_ERR_MEMORY] == 1, "memory error counted");
    expect(strcmp(em.last_message, "SEGV pid=1 addr=999") == 0, "last message");
    expect(em.last_category == JVK_ERR_MEMORY, "last category");
    expect(ic.panic == 0, "memory error does not panic");

    em_record(&em, JVK_ERR_SYSTEM, "manual panic", &ic);
    expect(em.counts[JVK_ERR_SYSTEM] == 1, "system error counted");
    expect(ic.panic == 1, "system error panics");

    /* ---- irq parse / name --------------------------------------------- */
    jvk_irq_t parsed;
    expect(jvk_irq_parse("page_fault", &parsed) == 1 && parsed == JVK_IRQ_PAGE_FAULT, "parse page_fault");
    expect(jvk_irq_parse("bogus", &parsed) == 0, "parse bogus rejected");
    expect(strcmp(jvk_irq_name(JVK_IRQ_DISK), "disk") == 0, "name disk");

    if (g_failures == 0) {
        printf("PASS: interrupt controller smoke test\n");
        return 0;
    }
    printf("interrupt controller smoke test FAILED (%d assertion(s))\n", g_failures);
    return 1;
}
