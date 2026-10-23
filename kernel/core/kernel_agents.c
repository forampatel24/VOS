#include <stdio.h>
#include <string.h>

#include "kernel_agents_internal.h"

static const cJSON* get_item(const cJSON* obj, const char* a, const char* b)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, a);
    if (!cJSON_IsString(item) && !cJSON_IsNumber(item) && b != NULL) {
        item = cJSON_GetObjectItemCaseSensitive(obj, b);
    }
    return item;
}

int kagent_boot(jvk_agent_registry_t* reg, const cJSON* config_root)
{
    agent_registry_init(reg);
    const cJSON* cfg = cJSON_GetObjectItemCaseSensitive(config_root, "agents");
    kagent_apply_config(reg, cfg);
    int builtins = 1;
    if (cJSON_IsObject(cfg)) {
        const cJSON* flag = get_item(cfg, "builtins", "seedBuiltins");
        if (cJSON_IsBool(flag)) {
            builtins = cJSON_IsTrue(flag) ? 1 : 0;
        }
    }
    if (builtins) {
        (void)agent_seed_builtins(reg);
    }
    if (cJSON_IsObject(cfg)) {
        const cJSON* defs = cJSON_GetObjectItemCaseSensitive(cfg, "definitions");
        if (cJSON_IsArray(defs)) {
            const cJSON* item = NULL;
            cJSON_ArrayForEach(item, defs) {
                jvk_agent_def_t def;
                if (kagent_parse_def(item, &def)) {
                    (void)agent_define(reg, &def);
                }
            }
        }
    }
    return reg->def_count;
}

void kagent_reset(jvk_agent_registry_t* reg)
{
    agent_registry_init(reg);
}

static int launch_agent(jvk_agent_registry_t* reg, kagent_env_t* env,
                        const cJSON* req, cJSON* result,
                        unsigned long ticks)
{
    int def_id = kagent_resolve_def(reg, req);
    const jvk_agent_def_t* def = agent_def_by_id(reg, def_id);
    if (def == NULL) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "unknown agent");
        return 1;
    }
    int running = agent_running_def(reg, def_id);
    if (running > 0) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "agent already running");
        cJSON_AddNumberToObject(result, "pid", running);
        return 1;
    }
    if (env->fs != NULL) {
        char dir[JVK_FS_PATH_LEN];
        snprintf(dir, sizeof(dir), "/Agents/%s", def->key);
        int dir_id = 0;
        if (!vfs_resolve(env->fs, dir, &dir_id)) {
            (void)vfs_mkdir(env->fs, dir, 0, 0777u, ticks, NULL);
        }
    }
    int priority = def->priority;
    int mem_pages = def->mem_pages;
    int burst = 32;
    const cJSON* prio = get_item(req, "priority", NULL);
    if (cJSON_IsNumber(prio)) {
        priority = (int)prio->valuedouble;
    }
    const cJSON* pages = get_item(req, "mem_pages", "memPages");
    if (cJSON_IsNumber(pages)) {
        mem_pages = (int)pages->valuedouble;
    }
    const cJSON* burst_item = get_item(req, "burst", "burst_time");
    if (cJSON_IsNumber(burst_item)) {
        burst = (int)burst_item->valuedouble;
    }
    if (priority < 0) priority = 0;
    if (priority > 10) priority = 10;
    if (mem_pages < 1) mem_pages = 1;
    if (mem_pages > 32) mem_pages = 32;
    if (burst < 1) burst = 1;

    int pid = 0;
    if (!pm_create(env->pm, def->name, priority, burst, ticks, &pid)) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "process table full");
        return 1;
    }
    if (!scheduler_register_ex(env->sched, pid, def->name, priority, burst,
                               ticks)) {
        (void)pm_kill(env->pm, pid);
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "scheduler full");
        return 1;
    }
    (void)pm_set_budget(env->pm, pid, mem_pages + 2);
    (void)mm_set_budget(env->mm, pid, mem_pages + 2);
    mm_alloc_result_t alloc;
    mm_status_t status = mm_alloc(env->mm, pid, mem_pages, &alloc);
    if (status != MM_OK) {
        (void)scheduler_unregister(env->sched, pid);
        (void)pm_kill(env->pm, pid);
        (void)mm_free_pid(env->mm, pid);
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", mm_status_name(status));
        return 1;
    }
    if (!agent_launch(reg, def_id, pid, ticks)) {
        (void)scheduler_unregister(env->sched, pid);
        (void)pm_kill(env->pm, pid);
        (void)mm_free_pid(env->mm, pid);
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "agent table full");
        return 1;
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "pid", pid);
    cJSON_AddNumberToObject(result, "def_id", def->id);
    cJSON_AddStringToObject(result, "key", def->key);
    cJSON_AddStringToObject(result, "agent", def->name);
    cJSON_AddNumberToObject(result, "priority", priority);
    cJSON_AddNumberToObject(result, "mem_pages", mem_pages);
    return 1;
}

static int define_agent(jvk_agent_registry_t* reg, const cJSON* req,
                        cJSON* result)
{
    jvk_agent_def_t def;
    if (!kagent_parse_def(req, &def)) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "name is required");
        return 1;
    }
    int id = agent_define(reg, &def);
    if (id <= 0) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error",
                                "definition table full or builtin key");
        return 1;
    }
    const jvk_agent_def_t* saved = agent_def_by_id(reg, id);
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "id", id);
    cJSON_AddStringToObject(result, "key", saved->key);
    cJSON_AddStringToObject(result, "name", saved->name);
    cJSON_AddNumberToObject(result, "tools", saved->tool_count);
    return 1;
}

static int remove_agent(jvk_agent_registry_t* reg, const cJSON* req,
                        cJSON* result)
{
    int def_id = kagent_resolve_def(reg, req);
    if (def_id <= 0 || !agent_remove(reg, def_id)) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error",
                                "unknown, builtin or running agent");
        return 1;
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "id", def_id);
    return 1;
}

static int queue_task(jvk_agent_registry_t* reg, const cJSON* req,
                      cJSON* result, unsigned long ticks)
{
    int pid = kagent_resolve_pid(reg, req);
    const cJSON* title = get_item(req, "title", "name");
    const cJSON* input = get_item(req, "input", "prompt");
    if (pid <= 0) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "agent not running");
        return 1;
    }
    int task_id = 0;
    const char* label = cJSON_IsString(title) ? title->valuestring
                                               : "Agent task";
    const char* text = cJSON_IsString(input) ? input->valuestring : "";
    if (!agent_queue_task(reg, pid, label, text, ticks, &task_id)) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "task queue full");
        return 1;
    }
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "pid", pid);
    cJSON_AddNumberToObject(result, "task_id", task_id);
    cJSON_AddStringToObject(result, "title", label);
    return 1;
}

static int cancel_task(jvk_agent_registry_t* reg, const cJSON* req,
                       cJSON* result, unsigned long ticks)
{
    const cJSON* id_item = get_item(req, "task", "task_id");
    if (!cJSON_IsNumber(id_item)) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "task is required");
        return 1;
    }
    int task_id = (int)id_item->valuedouble;
    jvk_agent_instance_t* inst =
        agent_instance_by_task(reg, task_id);
    int pid = kagent_resolve_pid(reg, req);
    if (pid <= 0 && inst != NULL) {
        pid = inst->pid;
    }
    if (pid <= 0 || !agent_cancel_task(reg, pid, task_id)) {
        cJSON_AddBoolToObject(result, "ok", 0);
        cJSON_AddStringToObject(result, "error", "task not found");
        return 1;
    }
    (void)ticks;
    cJSON_AddBoolToObject(result, "ok", 1);
    cJSON_AddNumberToObject(result, "task_id", task_id);
    cJSON_AddNumberToObject(result, "pid", pid);
    return 1;
}

int kagent_handle(jvk_agent_registry_t* reg, kagent_env_t* env,
                  const char* action, const cJSON* req, cJSON* result,
                  unsigned long ticks)
{
    if (strcmp(action, "list_agents") == 0) {
        kagent_snapshot(reg, env != NULL ? env->pm : NULL, 0, result);
        cJSON_AddBoolToObject(result, "ok", 1);
        return 1;
    }
    if (strcmp(action, "agent_list") == 0) {
        kagent_snapshot(reg, env != NULL ? env->pm : NULL, 0, result);
        cJSON_AddBoolToObject(result, "ok", 1);
        return 1;
    }
    if (strcmp(action, "agent_tasks") == 0) {
        int pid = kagent_resolve_pid(reg, req);
        kagent_tasks_json(reg, result, pid > 0 ? pid : 0);
        cJSON_AddBoolToObject(result, "ok", 1);
        return 1;
    }
    if (strcmp(action, "agent_define") == 0) {
        return define_agent(reg, req, result);
    }
    if (strcmp(action, "agent_remove") == 0) {
        return remove_agent(reg, req, result);
    }
    if (strcmp(action, "agent_launch") == 0) {
        return launch_agent(reg, env, req, result, ticks);
    }
    if (strcmp(action, "agent_task") == 0) {
        return queue_task(reg, req, result, ticks);
    }
    if (strcmp(action, "agent_cancel") == 0) {
        return cancel_task(reg, req, result, ticks);
    }
    if (strcmp(action, "agent_kill") == 0) {
        int pid = kagent_resolve_pid(reg, req);
        const cJSON* pid_item = get_item(req, "pid", NULL);
        if (pid <= 0 && cJSON_IsNumber(pid_item)) {
            pid = (int)pid_item->valuedouble;
        }
        if (pid <= 0 || env->kill == NULL ||
            agent_instance_by_pid(reg, pid) == NULL) {
            cJSON_AddBoolToObject(result, "ok", 0);
            cJSON_AddStringToObject(result, "error", "agent not running");
            return 1;
        }
        env->kill(env->kill_user, pid);
        cJSON_AddBoolToObject(result, "ok", 1);
        cJSON_AddNumberToObject(result, "pid", pid);
        return 1;
    }
    cJSON_AddBoolToObject(result, "ok", 0);
    cJSON_AddStringToObject(result, "error", "unknown agent action");
    return 1;
}
