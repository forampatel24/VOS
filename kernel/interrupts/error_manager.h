/*
 * kernel/interrupts/error_manager.h
 *
 * JARVIS OS — error categories, recovery and panic bridge.
 *
 * Recoverable errors return {"ok":false} and are counted; unrecoverable
 * ones set the interrupt controller's panic flag so the snapshot exposes
 * the failure without crashing the host process.
 */

#ifndef JARVIS_ERROR_MANAGER_H
#define JARVIS_ERROR_MANAGER_H

#include "ic.h"

typedef enum {
    JVK_ERR_NONE    = 0,
    JVK_ERR_MEMORY  = 1, /* segfault, OOM */
    JVK_ERR_PROCESS = 2, /* bad pid, table full */
    JVK_ERR_CPU     = 3, /* invalid opcode */
    JVK_ERR_SYSTEM  = 4, /* panic */
    JVK_ERR_COUNT   = 5,
} jvk_error_category_t;

typedef struct {
    unsigned long counts[JVK_ERR_COUNT];
    char          last_message[128];
    jvk_error_category_t last_category;
} jvk_error_manager_t;

void        em_init(jvk_error_manager_t* em);
void        em_record(jvk_error_manager_t* em, jvk_error_category_t cat, const char* msg, jvk_ic_t* ic);
const char* em_category_name(jvk_error_category_t cat);
void        em_snapshot(const jvk_error_manager_t* em, struct cJSON* root);

#endif /* JARVIS_ERROR_MANAGER_H */
