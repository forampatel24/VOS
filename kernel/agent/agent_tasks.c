#include <stdio.h>
#include <string.h>

#include "agent.h"

static void copy_text(char* dst, int cap, const char* src)
{
    if (cap <= 0) {
        return;
    }
    snprintf(dst, (size_t)cap, "%s", src == NULL ? "" : src);
}

const char* agent_stage_name(jvk_task_stage_t stage)
{
    switch (stage) {
    case JVK_TASK_QUEUED:    return "QUEUED";
    case JVK_TASK_PLANNING:  return "PLANNING";
    case JVK_TASK_THINKING:  return "THINKING";
    case JVK_TASK_ACTING:    return "ACTING";
    case JVK_TASK_REPORTING: return "REPORTING";
    case JVK_TASK_DONE:      return "DONE";
    case JVK_TASK_FAILED:    return "FAILED";
    case JVK_TASK_CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

const char* agent_activity(jvk_task_stage_t stage)
{
    switch (stage) {
    case JVK_TASK_QUEUED:    return "IDLE";
    case JVK_TASK_PLANNING:  return "PLANNING";
    case JVK_TASK_THINKING:  return "THINKING";
    case JVK_TASK_ACTING:    return "ACTING";
    case JVK_TASK_REPORTING: return "REPORTING";
    case JVK_TASK_DONE:      return "IDLE";
    case JVK_TASK_FAILED:    return "IDLE";
    case JVK_TASK_CANCELLED: return "IDLE";
    }
    return "IDLE";
}

static const jvk_agent_def_t* def_of(const jvk_agent_registry_t* reg,
                                     const jvk_agent_instance_t* inst)
{
    return agent_def_by_id(reg, inst->def_id);
}

int agent_launch(jvk_agent_registry_t* reg, int def_id, int pid,
                 unsigned long ticks)
{
    if (reg == NULL || pid <= 0 || agent_running_def(reg, def_id)) {
        return 0;
    }
    const jvk_agent_def_t* def = agent_def_by_id(reg, def_id);
    if (def == NULL || reg->live_count >= JVK_AGENT_MAX_LIVE) {
        return 0;
    }
    for (int i = 0; i < JVK_AGENT_MAX_LIVE; i++) {
        jvk_agent_instance_t* inst = &reg->instances[i];
        if (inst->used) {
            continue;
        }
        memset(inst, 0, sizeof(*inst));
        inst->used = 1;
        inst->pid = pid;
        inst->def_id = def_id;
        copy_text(inst->key, sizeof(inst->key), def->key);
        copy_text(inst->name, sizeof(inst->name), def->name);
        inst->priority = def->priority;
        inst->mem_pages = def->mem_pages;
        inst->launched_ticks = ticks;
        reg->live_count++;
        reg->stat_launched++;
        char msg[160];
        snprintf(msg, sizeof(msg), "AGENT_LAUNCHED pid=%d agent=%s key=%s",
                 pid, def->name, def->key);
        agent_emit(reg, msg);
        return 1;
    }
    return 0;
}

jvk_agent_instance_t* agent_instance_by_pid(jvk_agent_registry_t* reg,
                                            int pid)
{
    if (reg == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_AGENT_MAX_LIVE; i++) {
        if (reg->instances[i].used && reg->instances[i].pid == pid) {
            return &reg->instances[i];
        }
    }
    return NULL;
}

jvk_agent_instance_t* agent_instance_by_def(jvk_agent_registry_t* reg,
                                            int def_id)
{
    if (reg == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_AGENT_MAX_LIVE; i++) {
        if (reg->instances[i].used && reg->instances[i].def_id == def_id) {
            return &reg->instances[i];
        }
    }
    return NULL;
}

jvk_agent_instance_t* agent_instance_by_task(jvk_agent_registry_t* reg,
                                             int task_id)
{
    if (reg == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_AGENT_MAX_LIVE; i++) {
        jvk_agent_instance_t* inst = &reg->instances[i];
        if (!inst->used) {
            continue;
        }
        for (int j = 0; j < JVK_AGENT_MAX_TASKS; j++) {
            if (inst->tasks[j].id == task_id) {
                return inst;
            }
        }
    }
    return NULL;
}

static jvk_agent_task_t* task_by_id(jvk_agent_instance_t* inst, int task_id)
{
    if (inst == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_AGENT_MAX_TASKS; i++) {
        if (inst->tasks[i].id == task_id) {
            return &inst->tasks[i];
        }
    }
    return NULL;
}

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

static jvk_agent_task_t* free_slot(jvk_agent_instance_t* inst)
{
    for (int i = 0; i < JVK_AGENT_MAX_TASKS; i++) {
        if (inst->tasks[i].id == 0) {
            return &inst->tasks[i];
        }
    }
    jvk_agent_task_t* oldest = NULL;
    for (int i = 0; i < JVK_AGENT_MAX_TASKS; i++) {
        jvk_agent_task_t* task = &inst->tasks[i];
        if (!is_terminal(task->stage)) {
            continue;
        }
        if (oldest == NULL ||
            task->completed_ticks < oldest->completed_ticks) {
            oldest = task;
        }
    }
    return oldest;
}

static int pick_tool(const jvk_agent_def_t* def, const jvk_agent_task_t* task)
{
    if (def == NULL || def->tool_count <= 0) {
        return -1;
    }
    struct {
        const char* needle;
        const char* tool;
    } hints[] = {
        {"budget", "query_budget"}, {"total", "compute_total"},
        {"spec", "read_spec"}, {"code", "generate_code"},
        {"test", "run_tests"}, {"search", "search_files"},
        {"find", "search_files"}, {"read", "read_file"},
        {"summar", "summarize"}, {"draft", "write_draft"},
        {"rewrite", "rewrite"}, {"campaign", "campaign_report"},
        {"clause", "extract_clauses"}, {"policy", "policy_lookup"},
        {"trip", "plan_itinerary"}, {"health", "wellness_checklist"},
    };
    for (int i = 0; i < (int)(sizeof(hints) / sizeof(hints[0])); i++) {
        if (strstr(task->title, hints[i].needle) == NULL &&
            strstr(task->input, hints[i].needle) == NULL) {
            continue;
        }
        for (int j = 0; j < def->tool_count; j++) {
            if (strcmp(def->tools[j], hints[i].tool) == 0) {
                return j;
            }
        }
    }
    return (task->id + task->pid) % def->tool_count;
}

static unsigned int fnv1a(const char* text)
{
    unsigned int hash = 2166136261u;
    for (int i = 0; text != NULL && text[i] != '\0'; i++) {
        hash ^= (unsigned char)text[i];
        hash *= 16777619u;
    }
    return hash;
}

static void begin_next(jvk_agent_registry_t* reg,
                       jvk_agent_instance_t* inst, unsigned long ticks)
{
    if (inst->current_task != 0) {
        return;
    }
    jvk_agent_task_t* next = NULL;
    for (int i = 0; i < JVK_AGENT_MAX_TASKS; i++) {
        jvk_agent_task_t* task = &inst->tasks[i];
        if (task->id == 0 || task->stage != JVK_TASK_QUEUED) {
            continue;
        }
        if (next == NULL || task->id < next->id) {
            next = task;
        }
    }
    if (next == NULL) {
        return;
    }
    inst->current_task = next->id;
    next->started_ticks = ticks;
    char msg[160];
    snprintf(msg, sizeof(msg), "AGENT_TASK_STARTED pid=%d task=%d title=%s",
             inst->pid, next->id, next->title);
    agent_emit(reg, msg);
}

int agent_queue_task(jvk_agent_registry_t* reg, int pid,
                     const char* title, const char* input,
                     unsigned long ticks, int* out_id)
{
    jvk_agent_instance_t* inst = agent_instance_by_pid(reg, pid);
    if (inst == NULL || active_tasks(inst) >= reg->max_tasks) {
        return 0;
    }
    jvk_agent_task_t* task = free_slot(inst);
    if (task == NULL) {
        return 0;
    }
    memset(task, 0, sizeof(*task));
    task->id = reg->next_task_id++;
    task->def_id = inst->def_id;
    task->pid = pid;
    task->stage = JVK_TASK_QUEUED;
    task->created_ticks = ticks;
    copy_text(task->title, sizeof(task->title),
              title == NULL || title[0] == '\0' ? "Agent task" : title);
    copy_text(task->input, sizeof(task->input), input);
    inst->task_count++;
    inst->tasks_queued++;
    reg->stat_tasks_queued++;
    begin_next(reg, inst, ticks);
    char msg[192];
    snprintf(msg, sizeof(msg), "AGENT_TASK_QUEUED pid=%d task=%d title=%s",
             pid, task->id, task->title);
    agent_emit(reg, msg);
    if (out_id != NULL) {
        *out_id = task->id;
    }
    return 1;
}

static int stage_duration(const jvk_agent_registry_t* reg,
                          jvk_task_stage_t stage)
{
    int value = 1;
    switch (stage) {
    case JVK_TASK_PLANNING:  value = reg->stage_plan; break;
    case JVK_TASK_THINKING:  value = reg->stage_think; break;
    case JVK_TASK_ACTING:    value = reg->stage_act; break;
    case JVK_TASK_REPORTING: value = reg->stage_report; break;
    default:                 value = 1; break;
    }
    return value < 1 ? 1 : value;
}

static int progress_for(const jvk_agent_registry_t* reg,
                        const jvk_agent_task_t* task)
{
    int dur = stage_duration(reg, task->stage);
    int part = dur > 0 ? (task->stage_ticks * 100) / dur : 100;
    switch (task->stage) {
    case JVK_TASK_QUEUED:    return 0;
    case JVK_TASK_PLANNING:  return 10 + (24 * part) / 100;
    case JVK_TASK_THINKING:  return 35 + (24 * part) / 100;
    case JVK_TASK_ACTING:    return 60 + (24 * part) / 100;
    case JVK_TASK_REPORTING: return 85 + (14 * part) / 100;
    case JVK_TASK_DONE:      return 100;
    case JVK_TASK_FAILED:    return 100;
    case JVK_TASK_CANCELLED: return 100;
    }
    return 0;
}

static void build_plan(jvk_agent_registry_t* reg, jvk_agent_instance_t* inst,
                       jvk_agent_task_t* task)
{
    const jvk_agent_def_t* def = def_of(reg, inst);
    int tool = pick_tool(def, task);
    if (tool >= 0) {
        copy_text(task->tool, sizeof(task->tool), def->tools[tool]);
    } else {
        copy_text(task->tool, sizeof(task->tool), "simulate");
    }
    snprintf(task->plan, sizeof(task->plan),
             "1. Parse request \"%s\"\n2. Select tool %s\n"
             "3. Execute the simulated tool\n4. Store the report in /Agents",
             task->title, task->tool);
}

static void build_thought(jvk_agent_registry_t* reg,
                          jvk_agent_instance_t* inst,
                          jvk_agent_task_t* task)
{
    const jvk_agent_def_t* def = def_of(reg, inst);
    int words = 1;
    for (const char* p = task->input; *p != '\0'; p++) {
        if (*p == ' ') {
            words++;
        }
    }
    int confidence = 60 + (int)(fnv1a(task->input) % 40u);
    snprintf(task->thought, sizeof(task->thought),
             "Parsed %d word(s) for %s; priority %d; memory %d page(s); "
             "confidence %d%%",
             words, def != NULL ? def->personality : "agent",
             inst->priority, inst->mem_pages, confidence);
}

static void build_report(jvk_agent_registry_t* reg,
                         jvk_agent_instance_t* inst,
                         jvk_agent_task_t* task)
{
    const jvk_agent_def_t* def = def_of(reg, inst);
    int confidence = 60 + (int)(fnv1a(task->input) % 40u);
    snprintf(task->report, sizeof(task->report),
             "# %s\n\nAgent: %s\nRole: %s\nTask #%d: %s\nInput: %s\n"
             "Plan: %s\nTool: %s\nResult: %s\nConfidence: %d%%\n"
             "Reads: %d  Writes: %d\n",
             task->title, inst->name,
             def != NULL ? def->role : "simulated agent",
             task->id, task->title, task->input,
             task->thought, task->tool, task->result,
             confidence, task->reads, task->writes);
}

int agent_step(jvk_agent_registry_t* reg, int pid,
               unsigned long ticks, jvk_agent_effect_t* effect)
{
    if (effect != NULL) {
        memset(effect, 0, sizeof(*effect));
    }
    jvk_agent_instance_t* inst = agent_instance_by_pid(reg, pid);
    if (inst == NULL || inst->current_task == 0) {
        return 0;
    }
    jvk_agent_task_t* task = task_by_id(inst, inst->current_task);
    if (task == NULL || is_terminal(task->stage)) {
        return 0;
    }
    inst->cpu_ticks++;
    task->stage_ticks++;
    if (task->stage_ticks < stage_duration(reg, task->stage)) {
        task->progress = progress_for(reg, task);
        return 0;
    }
    task->stage_ticks = 0;
    if (task->stage == JVK_TASK_QUEUED) {
        task->stage = JVK_TASK_PLANNING;
        build_plan(reg, inst, task);
    } else if (task->stage == JVK_TASK_PLANNING) {
        task->stage = JVK_TASK_THINKING;
        build_thought(reg, inst, task);
    } else if (task->stage == JVK_TASK_THINKING) {
        task->stage = JVK_TASK_ACTING;
        snprintf(task->action, sizeof(task->action), "%s(\"%s\")",
                 task->tool, task->input);
        if (task->result[0] == '\0') {
            snprintf(task->result, sizeof(task->result),
                     "simulated %s completed", task->tool);
        }
        if (effect != NULL) {
            effect->kind = JVK_AGENT_EFFECT_TOOL;
            effect->pid = inst->pid;
            effect->def_id = inst->def_id;
            effect->task_id = task->id;
            copy_text(effect->key, sizeof(effect->key), inst->key);
            copy_text(effect->agent, sizeof(effect->agent), inst->name);
            copy_text(effect->tool, sizeof(effect->tool), task->tool);
            copy_text(effect->input, sizeof(effect->input), task->input);
        }
    } else if (task->stage == JVK_TASK_ACTING) {
        task->stage = JVK_TASK_REPORTING;
        build_report(reg, inst, task);
    } else if (task->stage == JVK_TASK_REPORTING) {
        task->stage = JVK_TASK_DONE;
        task->completed_ticks = ticks;
        task->progress = 100;
        inst->current_task = 0;
        inst->tasks_done++;
        reg->stat_tasks_done++;
        reg->stat_reports++;
        char msg[192];
        snprintf(msg, sizeof(msg), "AGENT_TASK_DONE pid=%d task=%d tool=%s",
                 inst->pid, task->id, task->tool);
        agent_emit(reg, msg);
        if (effect != NULL) {
            effect->kind = JVK_AGENT_EFFECT_REPORT;
            effect->pid = inst->pid;
            effect->def_id = inst->def_id;
            effect->task_id = task->id;
            copy_text(effect->key, sizeof(effect->key), inst->key);
            copy_text(effect->agent, sizeof(effect->agent), inst->name);
            copy_text(effect->report, sizeof(effect->report), task->report);
        }
        begin_next(reg, inst, ticks);
        return 1;
    }
    task->progress = progress_for(reg, task);
    char msg[160];
    snprintf(msg, sizeof(msg), "AGENT_STAGE pid=%d task=%d stage=%s",
             inst->pid, task->id, agent_stage_name(task->stage));
    agent_emit(reg, msg);
    return effect != NULL && effect->kind != JVK_AGENT_EFFECT_NONE;
}

void agent_apply_tool_result(jvk_agent_registry_t* reg, int pid,
                             int task_id, const char* result,
                             int writes, int reads)
{
    jvk_agent_instance_t* inst = agent_instance_by_pid(reg, pid);
    jvk_agent_task_t* task = task_by_id(inst, task_id);
    if (task == NULL) {
        return;
    }
    copy_text(task->result, sizeof(task->result),
              result == NULL || result[0] == '\0' ? "completed" : result);
    task->writes += writes;
    task->reads += reads;
    snprintf(task->action, sizeof(task->action), "%s -> %s",
             task->tool, task->result);
    char msg[192];
    snprintf(msg, sizeof(msg), "AGENT_TOOL pid=%d task=%d tool=%s result=%s",
             pid, task_id, task->tool, task->result);
    agent_emit(reg, msg);
}

void agent_add_memory(jvk_agent_registry_t* reg, int pid, int pages)
{
    jvk_agent_instance_t* inst = agent_instance_by_pid(reg, pid);
    if (inst == NULL || pages <= 0) {
        return;
    }
    inst->memory_growth += pages;
    inst->mem_pages += pages;
}

int agent_cancel_task(jvk_agent_registry_t* reg, int pid, int task_id)
{
    jvk_agent_instance_t* inst = agent_instance_by_pid(reg, pid);
    jvk_agent_task_t* task = task_by_id(inst, task_id);
    if (task == NULL || is_terminal(task->stage)) {
        return 0;
    }
    if (inst->current_task == task->id) {
        inst->current_task = 0;
    }
    task->stage = JVK_TASK_CANCELLED;
    task->progress = 100;
    task->completed_ticks = 0;
    inst->tasks_failed++;
    reg->stat_tasks_failed++;
    char msg[160];
    snprintf(msg, sizeof(msg), "AGENT_TASK_CANCELLED pid=%d task=%d",
             pid, task_id);
    agent_emit(reg, msg);
    begin_next(reg, inst, 0);
    return 1;
}

void agent_on_kill(jvk_agent_registry_t* reg, int pid)
{
    jvk_agent_instance_t* inst = agent_instance_by_pid(reg, pid);
    if (inst == NULL) {
        return;
    }
    for (int i = 0; i < JVK_AGENT_MAX_TASKS; i++) {
        jvk_agent_task_t* task = &inst->tasks[i];
        if (task->id == 0 || is_terminal(task->stage)) {
            continue;
        }
        task->stage = JVK_TASK_FAILED;
        task->progress = 100;
        inst->tasks_failed++;
        reg->stat_tasks_failed++;
    }
    char msg[160];
    snprintf(msg, sizeof(msg), "AGENT_KILLED pid=%d agent=%s",
             pid, inst->name);
    agent_emit(reg, msg);
    if (reg->live_count > 0) {
        reg->live_count--;
    }
    memset(inst, 0, sizeof(*inst));
}
