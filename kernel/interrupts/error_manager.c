#include <stddef.h>
/*
 * kernel/interrupts/error_manager.c
 *
 * JARVIS OS — error manager (counts, last error, panic escalation).
 */

#include <string.h>
#include <stdio.h>

#include "deps/cJSON/cJSON.h"
#include "error_manager.h"

void em_init(jvk_error_manager_t* em)
{
    if (em == NULL) {
        return;
    }
    memset(em, 0, sizeof(*em));
}

void em_record(jvk_error_manager_t* em, jvk_error_category_t cat, const char* msg, jvk_ic_t* ic)
{
    if (em == NULL) {
        return;
    }
    if (cat >= 0 && cat < JVK_ERR_COUNT) {
        em->counts[cat]++;
        em->last_category = cat;
    }
    snprintf(em->last_message, sizeof(em->last_message), "%s", msg ? msg : "");
    if (cat == JVK_ERR_SYSTEM && ic != NULL) {
        ic_set_panic(ic, msg);
    }
}

const char* em_category_name(jvk_error_category_t cat)
{
    switch (cat) {
    case JVK_ERR_MEMORY:  return "memory";
    case JVK_ERR_PROCESS: return "process";
    case JVK_ERR_CPU:     return "cpu";
    case JVK_ERR_SYSTEM:  return "system";
    default:              return "none";
    }
}

void em_snapshot(const jvk_error_manager_t* em, cJSON* root)
{
    cJSON* obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "last_category", em_category_name(em->last_category));
    cJSON_AddStringToObject(obj, "last_message", em->last_message);
    cJSON* counts = cJSON_CreateObject();
    for (int i = 0; i < JVK_ERR_COUNT; i++) {
        cJSON_AddNumberToObject(counts, em_category_name((jvk_error_category_t)i), (double)em->counts[i]);
    }
    cJSON_AddItemToObject(obj, "counts", counts);
    cJSON_AddItemToObject(root, "error_manager", obj);
}
