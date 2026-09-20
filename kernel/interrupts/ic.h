/*
 * kernel/interrupts/ic.h
 *
 * JARVIS OS — interrupt controller (priority queue + panic flag).
 *
 * The controller is the only authority for interrupts (Interrupts ↓ Queue
 * ↓ Priority ↓ ISR ↓ Resume). Subsystems enqueue through the kernel;
 * the controller decides order and guarantees one ISR at a time.
 */

#ifndef JARVIS_IC_H
#define JARVIS_IC_H

#include "interrupt_types.h"

#define JVK_IC_CAP 32
#define JVK_IC_SOURCE_LEN 32
#define JVK_IC_DETAIL_LEN 64
#define JVK_IC_PANIC_LEN  128

typedef struct {
    jvk_irq_t          irq;
    int                priority;
    char               source[JVK_IC_SOURCE_LEN];
    char               detail[JVK_IC_DETAIL_LEN];
    unsigned long      enqueued_ticks;
    jvk_irq_status_t   status;
} jvk_irq_entry_t;

typedef struct {
    jvk_irq_entry_t entries[JVK_IC_CAP];
    int             count;
    unsigned long   handled;
    unsigned long   dropped;
    int             panic;
    char            panic_reason[JVK_IC_PANIC_LEN];
} jvk_ic_t;

void ic_init(jvk_ic_t* ic);
int  ic_enqueue(jvk_ic_t* ic, jvk_irq_t irq, const char* source,
                const char* detail, unsigned long ticks);
int  ic_dequeue_next(jvk_ic_t* ic, jvk_irq_entry_t* out);
int  ic_pending(const jvk_ic_t* ic);
int  ic_set_panic(jvk_ic_t* ic, const char* reason);
void ic_clear_panic(jvk_ic_t* ic);

struct cJSON;
void ic_snapshot(const jvk_ic_t* ic, struct cJSON* root);

#endif /* JARVIS_IC_H */
