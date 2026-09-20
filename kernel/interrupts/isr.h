/*
 * kernel/interrupts/isr.h
 *
 * JARVIS OS — ISR registry (one handler per IRQ, one ISR at a time).
 *
 * Handlers are registered at boot and invoked by the controller's
 * dequeue step. They run to completion before the next IRQ is taken.
 */

#ifndef JARVIS_ISR_H
#define JARVIS_ISR_H

#include "ic.h"

typedef void (*jvk_isr_fn)(jvk_ic_t* ic, const jvk_irq_entry_t* entry, void* user);

typedef struct {
    jvk_isr_fn fn;
    void*      user;
} jvk_isr_slot_t;

typedef struct {
    jvk_isr_slot_t slots[JVK_IRQ_COUNT];
} jvk_isr_registry_t;

void isr_init(jvk_isr_registry_t* r);
void isr_register(jvk_isr_registry_t* r, jvk_irq_t irq, jvk_isr_fn fn, void* user);
void isr_handle(jvk_isr_registry_t* r, jvk_ic_t* ic, const jvk_irq_entry_t* entry);
void isr_register_defaults(jvk_isr_registry_t* r);

#endif /* JARVIS_ISR_H */
