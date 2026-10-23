#ifndef JARVIS_AGENT_H
#define JARVIS_AGENT_H

#include "agent_types.h"

void        agent_registry_init(jvk_agent_registry_t* reg);
void        agent_set_observer(jvk_agent_registry_t* reg,
                               agent_event_fn fn, void* user);
void        agent_emit(jvk_agent_registry_t* reg, const char* message);

int         agent_seed_builtins(jvk_agent_registry_t* reg);
int         agent_define(jvk_agent_registry_t* reg,
                         const jvk_agent_def_t* def);
int         agent_remove(jvk_agent_registry_t* reg, int def_id);
int         agent_running_def(const jvk_agent_registry_t* reg, int def_id);

const jvk_agent_def_t* agent_def_by_key(const jvk_agent_registry_t* reg,
                                        const char* key);
const jvk_agent_def_t* agent_def_by_name(const jvk_agent_registry_t* reg,
                                         const char* name);
const jvk_agent_def_t* agent_def_by_id(const jvk_agent_registry_t* reg,
                                       int id);

int         agent_launch(jvk_agent_registry_t* reg, int def_id, int pid,
                         unsigned long ticks);
jvk_agent_instance_t* agent_instance_by_pid(jvk_agent_registry_t* reg,
                                            int pid);
jvk_agent_instance_t* agent_instance_by_def(jvk_agent_registry_t* reg,
                                            int def_id);
jvk_agent_instance_t* agent_instance_by_task(jvk_agent_registry_t* reg,
                                             int task_id);

int         agent_queue_task(jvk_agent_registry_t* reg, int pid,
                             const char* title, const char* input,
                             unsigned long ticks, int* out_id);
int         agent_cancel_task(jvk_agent_registry_t* reg, int pid,
                              int task_id);
int         agent_step(jvk_agent_registry_t* reg, int pid,
                       unsigned long ticks, jvk_agent_effect_t* effect);
void        agent_apply_tool_result(jvk_agent_registry_t* reg, int pid,
                                    int task_id, const char* result,
                                    int writes, int reads);
void        agent_add_memory(jvk_agent_registry_t* reg, int pid, int pages);
void        agent_on_kill(jvk_agent_registry_t* reg, int pid);

const char* agent_stage_name(jvk_task_stage_t stage);
const char* agent_activity(jvk_task_stage_t stage);

#endif
