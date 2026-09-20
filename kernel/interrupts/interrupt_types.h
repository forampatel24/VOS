/*
 * kernel/interrupts/interrupt_types.h
 *
 * JARVIS OS — interrupt types and priorities.
 *
 * Priority order (lower value = higher urgency):
 *   SHUTDOWN (0) > PAGE_FAULT (1) > DISK (2) > KEYBOARD (3)
 *   > TIMER (4) > SOFTWARE (5)
 * This models the textbook: faults and shutdown preempt periodic timers.
 */

#ifndef JARVIS_INTERRUPT_TYPES_H
#define JARVIS_INTERRUPT_TYPES_H

typedef enum {
    JVK_IRQ_SHUTDOWN    = 0,
    JVK_IRQ_PAGE_FAULT  = 1,
    JVK_IRQ_DISK        = 2,
    JVK_IRQ_KEYBOARD    = 3,
    JVK_IRQ_TIMER       = 4,
    JVK_IRQ_SOFTWARE    = 5,
    JVK_IRQ_COUNT       = 6,
} jvk_irq_t;

typedef enum {
    JVK_IRQ_PENDING   = 0,
    JVK_IRQ_SERVICING = 1,
    JVK_IRQ_SERVICED  = 2,
} jvk_irq_status_t;

static inline int jvk_irq_priority(jvk_irq_t irq)
{
    return (int)irq;
}

const char* jvk_irq_name(jvk_irq_t irq);
int         jvk_irq_parse(const char* name, jvk_irq_t* out);

#endif /* JARVIS_INTERRUPT_TYPES_H */
