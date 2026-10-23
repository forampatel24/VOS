#include <string.h>

#include "kernel_agents_internal.h"

static int is_terminal(jvk_task_stage_t stage)
{
    return stage == JVK_TASK_DONE || stage == JVK_TASK_FAILED ||
           stage == JVK_TASK_CANCELLED;
}

static int active_tasks(const jvk_agent_instance_t* inst)
{
    int count = 0;
    for (int i = 0; i < JVK_AGENT_MAX_TASKS; i++) {
        if (inst->tasks[i].id != 0 && !is_terminal(inst->tasks[i].stage)) {
            count++;
        }
    }
    return count;
}

static void def_json(const jvk_agent_def_t* def, cJSON* out, int running)
{
    cJSON_AddNumberToObject(out, "id", def->id);
    cJSON_AddStringToObject(out, "key", def->key);
    cJSON_AddStringToObject(out, "name", def->name);
    cJSON_AddStringToObject(out, "role", def->role);
    cJSON_AddStringToObject(out, "personality", def->personality);
    cJSON_AddStringToObject(out, "color", def->color);
    cJSON_AddStringToObject(out, "icon", def->icon);
    cJSON_AddBoolToObject(out, "builtin", def->builtin);
    cJSON_AddNumberToObject(out, "priority", def->priority);
    cJSON_AddNumberToObject(out, "mem_pages", def->mem_pages);
    cJSON_AddNumberToObject(out, "running_pid", running);
    cJSON* tools = cJSON_CreateArray();
    for (int i = 0; i < def->tool_count; i++) {
        cJSON_AddItemToArray(tools, cJSON_CreateString(def->tools[i]));
    }
    cJSON_AddItemToObject(out, "tools", tools);
}

static void task_json(const jvk_agent_registry_t* reg,
                      const jvk_agent_instance_t* inst,
                      const jvk_agent_task_t* task, cJSON* out)
{
    const jvk_agent_def_t* def = agent_def_by_id(reg, inst->def_id);
    cJSON_AddNumberToObject(out, "id", task->id);
    cJSON_AddNumberToObject(out, "pid", task->pid);
    cJSON_AddNumberToObject(out, "def_id", task->def_id);
    cJSON_AddStringToObject(out, "agent_key", inst->key);
    cJSON_AddStringToObject(out, "agent", inst->name);
    cJSON_AddStringToObject(out, "title", task->title);
    cJSON_AddStringToObject(out, "input", task->input);
    cJSON_AddStringToObject(out, "stage", agent_stage_name(task->stage));
    cJSON_AddNumberToObject(out, "progress", task->progress);
    cJSON_AddStringToObject(out, "tool", task->tool);
    cJSON_AddStringToObject(out, "plan", task->plan);
    cJSON_AddStringToObject(out, "thought", task->thought);
    cJSON_AddStringToObject(out, "action", task->action);
    cJSON_AddStringToObject(out, "result", task->result);
    cJSON_AddStringToObject(out, "report", task->report);
    cJSON_AddNumberToObject(out, "writes", task->writes);
    cJSON_AddNumberToObject(out, "reads", task->reads);
    cJSON_AddNumberToObject(out, "created_ticks",
                            (double)task->created_ticks);
    cJSON_AddNumberToObject(out, "started_ticks",
                            (double)task->started_ticks);
    cJSON_AddNumberToObject(out, "completed_ticks",
                            (double)task->completed_ticks);
    (void)def;
}

static void instance_json(const jvk_agent_registry_t* reg,
                          jvk_process_manager_t* pm, int current_pid,
                          const jvk_agent_instance_t* inst, cJSON* out)
{
    const jvk_agent_task_t* current = NULL;
    for (int i = 0; i < JVK_AGENT_MAX_TASKS; i++) {
        if (inst->tasks[i].id == inst->current_task) {
            current = &inst->tasks[i];
            break;
        }
    }
    cJSON_AddNumberToObject(out, "pid", inst->pid);
    cJSON_AddNumberToObject(out, "def_id", inst->def_id);
    cJSON_AddStringToObject(out, "key", inst->key);
    cJSON_AddStringToObject(out, "name", inst->name);
    jvk_pcb_t* pcb = pm != NULL ? pm_get(pm, inst->pid) : NULL;
    const char* state = pcb != NULL ? pm_state_name(pcb->state) : "TERMINATED";
    if (pcb != NULL && inst->pid == current_pid &&
        pcb->state == PROC_STATE_READY) {
        state = "RUNNING";
    }
    cJSON_AddStringToObject(out, "state", state);
    cJSON_AddStringToObject(out, "activity",
                            current != NULL ? agent_activity(current->stage)
                                            : "IDLE");
    cJSON_AddNumberToObject(out, "priority", inst->priority);
    cJSON_AddNumberToObject(out, "mem_pages", inst->mem_pages);
    cJSON_AddNumberToObject(out, "memory_growth", inst->memory_growth);
    cJSON_AddNumberToObject(out, "cpu_ticks", inst->cpu_ticks);
    cJSON_AddNumberToObject(out, "launched_ticks",
                            (double)inst->launched_ticks);
    cJSON_AddNumberToObject(out, "tasks_queued", (double)inst->tasks_queued);
    cJSON_AddNumberToObject(out, "tasks_done", (double)inst->tasks_done);
    cJSON_AddNumberToObject(out, "tasks_failed", (double)inst->tasks_failed);
    cJSON_AddNumberToObject(out, "active_tasks", active_tasks(inst));
    cJSON_AddNumberToObject(out, "current_task", inst->current_task);
    if (current != NULL) {
        cJSON* cur = cJSON_CreateObject();
        cJSON_AddNumberToObject(cur, "id", current->id);
        cJSON_AddStringToObject(cur, "title", current->title);
        cJSON_AddStringToObject(cur, "stage",
                                agent_stage_name(current->stage));
        cJSON_AddNumberToObject(cur, "progress", current->progress);
        cJSON_AddStringToObject(cur, "tool", current->tool);
        cJSON_AddItemToObject(out, "current", cur);
    }
    (void)reg;
}

void kagent_snapshot(const jvk_agent_registry_t* reg,
                     jvk_process_manager_t* pm, int current_pid,
                     cJSON* root)
{
    if (reg == NULL || root == NULL) {
        return;
    }
    cJSON* out = cJSON_CreateObject();
    cJSON* config = cJSON_CreateObject();
    cJSON_AddNumberToObject(config, "max_definitions", JVK_AGENT_MAX_DEFS);
    cJSON_AddNumberToObject(config, "max_live", JVK_AGENT_MAX_LIVE);
    cJSON_AddNumberToObject(config, "max_tasks", reg->max_tasks);
    cJSON_AddNumberToObject(config, "stage_plan", reg->stage_plan);
    cJSON_AddNumberToObject(config, "stage_think", reg->stage_think);
    cJSON_AddNumberToObject(config, "stage_act", reg->stage_act);
    cJSON_AddNumberToObject(config, "stage_report", reg->stage_report);
    cJSON_AddItemToObject(out, "config", config);

    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "definitions", reg->def_count);
    cJSON_AddNumberToObject(stats, "live", reg->live_count);
    cJSON_AddNumberToObject(stats, "launched", (double)reg->stat_launched);
    cJSON_AddNumberToObject(stats, "tasks_queued",
                            (double)reg->stat_tasks_queued);
    cJSON_AddNumberToObject(stats, "tasks_done",
                            (double)reg->stat_tasks_done);
    cJSON_AddNumberToObject(stats, "tasks_failed",
                            (double)reg->stat_tasks_failed);
    cJSON_AddNumberToObject(stats, "reports", (double)reg->stat_reports);
    cJSON_AddItemToObject(out, "stats", stats);

    cJSON* defs = cJSON_CreateArray();
    for (int i = 0; i < reg->def_count; i++) {
        cJSON* item = cJSON_CreateObject();
        def_json(&reg->defs[i], item, agent_running_def(reg, reg->defs[i].id));
        cJSON_AddItemToArray(defs, item);
    }
    cJSON_AddItemToObject(out, "definitions", defs);

    cJSON* instances = cJSON_CreateArray();
    for (int i = 0; i < JVK_AGENT_MAX_LIVE; i++) {
        if (!reg->instances[i].used) {
            continue;
        }
        cJSON* item = cJSON_CreateObject();
        instance_json(reg, pm, current_pid, &reg->instances[i], item);
        cJSON_AddItemToArray(instances, item);
    }
    cJSON_AddItemToObject(out, "instances", instances);

    cJSON* tasks = cJSON_CreateArray();
    for (int i = 0; i < JVK_AGENT_MAX_LIVE; i++) {
        const jvk_agent_instance_t* inst = &reg->instances[i];
        if (!inst->used) {
            continue;
        }
        for (int j = 0; j < JVK_AGENT_MAX_TASKS; j++) {
            if (inst->tasks[j].id == 0) {
                continue;
            }
            cJSON* item = cJSON_CreateObject();
            task_json(reg, inst, &inst->tasks[j], item);
            cJSON_AddItemToArray(tasks, item);
        }
    }
    cJSON_AddItemToObject(out, "tasks", tasks);

    cJSON_AddItemToObject(root, "agents", out);
}

void kagent_tasks_json(const jvk_agent_registry_t* reg, cJSON* result,
                       int pid)
{
    cJSON* tasks = cJSON_CreateArray();
    if (reg != NULL) {
        for (int i = 0; i < JVK_AGENT_MAX_LIVE; i++) {
            const jvk_agent_instance_t* inst = &reg->instances[i];
            if (!inst->used || (pid > 0 && inst->pid != pid)) {
                continue;
            }
            for (int j = 0; j < JVK_AGENT_MAX_TASKS; j++) {
                if (inst->tasks[j].id == 0) {
                    continue;
                }
                cJSON* item = cJSON_CreateObject();
                task_json(reg, inst, &inst->tasks[j], item);
                cJSON_AddItemToArray(tasks, item);
            }
        }
    }
    cJSON_AddItemToObject(result, "tasks", tasks);
}
