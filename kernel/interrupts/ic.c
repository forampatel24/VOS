/*
 * kernel/interrupts/ic.c
 *
 * JARVIS OS — interrupt controller (priority queue).
 *
 * Enqueue keeps the array sorted by (priority, enqueued_ticks) so the
 * head is always the next IRQ to service. One dequeue = one ISR.
 */

#include <stdio.h>
#include <string.h>

#include "deps/cJSON/cJSON.h"
#include "ic.h"

void ic_init(jvk_ic_t* ic)
{
    if (ic == NULL) {
        return;
    }
    memset(ic, 0, sizeof(*ic));
}

int ic_enqueue(jvk_ic_t* ic, jvk_irq_t irq, const char* source,
               const char* detail, unsigned long ticks)
{
    if (ic == NULL || ic->count >= JVK_IC_CAP) {
        if (ic != NULL) {
            ic->dropped++;
        }
        return 0;
    }
    if (irq < 0 || irq >= JVK_IRQ_COUNT) {
        return 0;
    }

    jvk_irq_entry_t e;
    memset(&e, 0, sizeof(e));
    e.irq             = irq;
    e.priority        = jvk_irq_priority(irq);
    e.enqueued_ticks  = ticks;
    e.status          = JVK_IRQ_PENDING;
    snprintf(e.source, sizeof(e.source), "%s", source ? source : jvk_irq_name(irq));
    snprintf(e.detail, sizeof(e.detail), "%s", detail ? detail : "");

    int pos = ic->count;
    for (int i = 0; i < ic->count; i++) {
        if (e.priority < ic->entries[i].priority ||
            (e.priority == ic->entries[i].priority &&
             e.enqueued_ticks < ic->entries[i].enqueued_ticks)) {
            pos = i;
            break;
        }
    }
    for (int i = ic->count; i > pos; i--) {
        ic->entries[i] = ic->entries[i - 1];
    }
    ic->entries[pos] = e;
    ic->count++;
    return 1;
}

int ic_dequeue_next(jvk_ic_t* ic, jvk_irq_entry_t* out)
{
    if (ic == NULL || ic->count == 0) {
        return 0;
    }
    if (out != NULL) {
        *out = ic->entries[0];
    }
    for (int i = 1; i < ic->count; i++) {
        ic->entries[i - 1] = ic->entries[i];
    }
    ic->count--;
    ic->handled++;
    return 1;
}

int ic_pending(const jvk_ic_t* ic)
{
    return ic ? ic->count : 0;
}

int ic_set_panic(jvk_ic_t* ic, const char* reason)
{
    if (ic == NULL) {
        return 0;
    }
    ic->panic = 1;
    snprintf(ic->panic_reason, sizeof(ic->panic_reason), "%s",
             reason ? reason : "kernel panic");
    return 1;
}

void ic_clear_panic(jvk_ic_t* ic)
{
    if (ic == NULL) {
        return;
    }
    ic->panic = 0;
    ic->panic_reason[0] = '\0';
}

void ic_snapshot(const jvk_ic_t* ic, cJSON* root)
{
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "pending", ic->count);
    cJSON_AddNumberToObject(obj, "handled", ic->handled);
    cJSON_AddNumberToObject(obj, "dropped", ic->dropped);
    cJSON_AddBoolToObject(obj, "panic", ic->panic);
    cJSON_AddStringToObject(obj, "panic_reason", ic->panic_reason);

    cJSON* queue = cJSON_CreateArray();
    for (int i = 0; i < ic->count; i++) {
        const jvk_irq_entry_t* e = &ic->entries[i];
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "irq", jvk_irq_name(e->irq));
        cJSON_AddNumberToObject(item, "priority", e->priority);
        cJSON_AddStringToObject(item, "source", e->source);
        cJSON_AddStringToObject(item, "detail", e->detail);
        cJSON_AddNumberToObject(item, "enqueued_ticks", (double)e->enqueued_ticks);
        const char* st = e->status == JVK_IRQ_PENDING ? "PENDING" :
                         e->status == JVK_IRQ_SERVICING ? "SERVICING" : "SERVICED";
        cJSON_AddStringToObject(item, "status", st);
        cJSON_AddItemToArray(queue, item);
    }
    cJSON_AddItemToObject(obj, "queue", queue);
    cJSON_AddItemToObject(root, "interrupts", obj);
}

const char* jvk_irq_name(jvk_irq_t irq)
{
    switch (irq) {
    case JVK_IRQ_SHUTDOWN:   return "shutdown";
    case JVK_IRQ_PAGE_FAULT: return "page_fault";
    case JVK_IRQ_DISK:       return "disk";
    case JVK_IRQ_KEYBOARD:   return "keyboard";
    case JVK_IRQ_TIMER:      return "timer";
    case JVK_IRQ_SOFTWARE:   return "software";
    default:                 return "unknown";
    }
}

int jvk_irq_parse(const char* name, jvk_irq_t* out)
{
    if (name == NULL || out == NULL) {
        return 0;
    }
    for (int i = 0; i < JVK_IRQ_COUNT; i++) {
        if (strcmp(name, jvk_irq_name((jvk_irq_t)i)) == 0) {
            *out = (jvk_irq_t)i;
            return 1;
        }
    }
    return 0;
}
