#ifndef JARVIS_KERNEL_AGENTS_H
#define JARVIS_KERNEL_AGENTS_H

#include "../agent/agent.h"
#include "../deps/cJSON/cJSON.h"
#include "../filesystem/vfs.h"
#include "../memory/memory_manager.h"
#include "../process/process_manager.h"
#include "../scheduler/scheduler.h"

typedef struct {
    jvk_process_manager_t* pm;
    jvk_scheduler_t*       sched;
    jvk_memory_manager_t*  mm;
    jvk_vfs_t*             fs;
    void                 (*kill)(void* user, int pid);
    void*                  kill_user;
} kagent_env_t;

int  kagent_boot(jvk_agent_registry_t* reg, const cJSON* config_root);
void kagent_reset(jvk_agent_registry_t* reg);

int  kagent_handle(jvk_agent_registry_t* reg, kagent_env_t* env,
                   const char* action, const cJSON* req, cJSON* result,
                   unsigned long ticks);

void kagent_tick(jvk_agent_registry_t* reg, int current_pid,
                 unsigned long ticks, jvk_vfs_t* fs,
                 jvk_memory_manager_t* mm);

void kagent_snapshot(const jvk_agent_registry_t* reg,
                     jvk_process_manager_t* pm, int current_pid,
                     cJSON* root);
void kagent_tasks_json(const jvk_agent_registry_t* reg, cJSON* result,
                       int pid);

void kagent_on_kill(jvk_agent_registry_t* reg, int pid);

#endif
