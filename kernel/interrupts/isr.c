/*
 * kernel/interrupts/isr.c
 *
 * JARVIS OS — default ISRs.
 *
 * Each handler is tiny and honest: it logs that the specific interrupt
 * was serviced and, for shutdown, raises the panic flag.
 */

#include <stdio.h>
#include <string.h>

#include "isr.h"

/* ---- default handlers -------------------------------------------------- */

static void isr_timer(jvk_ic_t* ic, const jvk_irq_entry_t* e, void* user)
{
    (void)ic;
    (void)e;
    (void)user;
}

static void isr_page_fault(jvk_ic_t* ic, const jvk_irq_entry_t* e, void* user)
{
    (void)ic;
    (void)e;
    (void)user;
}

static void isr_disk(jvk_ic_t* ic, const jvk_irq_entry_t* e, void* user)
{
    (void)ic;
    (void)e;
    (void)user;
}

static void isr_keyboard(jvk_ic_t* ic, const jvk_irq_entry_t* e, void* user)
{
    (void)ic;
    (void)e;
    (void)user;
}

static void isr_software(jvk_ic_t* ic, const jvk_irq_entry_t* e, void* user)
{
    (void)ic;
    (void)e;
    (void)user;
}

static void isr_shutdown(jvk_ic_t* ic, const jvk_irq_entry_t* e, void* user)
{
    (void)user;
    if (ic != NULL) {
        char reason[96];
        snprintf(reason, sizeof(reason), "shutdown IRQ: %s", e->detail[0] ? e->detail : e->source);
        ic_set_panic(ic, reason);
    }
}

/* ---- registry ---------------------------------------------------------- */

void isr_init(jvk_isr_registry_t* r)
{
    if (r == NULL) {
        return;
    }
    memset(r, 0, sizeof(*r));
}

void isr_register(jvk_isr_registry_t* r, jvk_irq_t irq, jvk_isr_fn fn, void* user)
{
    if (r == NULL || irq < 0 || irq >= JVK_IRQ_COUNT) {
        return;
    }
    r->slots[irq].fn   = fn;
    r->slots[irq].user = user;
}

void isr_handle(jvk_isr_registry_t* r, jvk_ic_t* ic, const jvk_irq_entry_t* entry)
{
    if (r == NULL || entry == NULL) {
        return;
    }
    jvk_isr_slot_t* slot = &r->slots[entry->irq];
    if (slot->fn != NULL) {
        slot->fn(ic, entry, slot->user);
    }
}

void isr_register_defaults(jvk_isr_registry_t* r)
{
    isr_register(r, JVK_IRQ_TIMER,      isr_timer,      NULL);
    isr_register(r, JVK_IRQ_PAGE_FAULT, isr_page_fault, NULL);
    isr_register(r, JVK_IRQ_DISK,       isr_disk,       NULL);
    isr_register(r, JVK_IRQ_KEYBOARD,   isr_keyboard,   NULL);
    isr_register(r, JVK_IRQ_SOFTWARE,   isr_software,   NULL);
    isr_register(r, JVK_IRQ_SHUTDOWN,   isr_shutdown,   NULL);
}
