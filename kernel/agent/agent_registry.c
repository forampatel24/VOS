#include <stdio.h>
#include <string.h>

#include "agent.h"

static void copy_text(char* dst, int cap, const char* src)
{
    if (cap <= 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, (size_t)cap, "%s", src);
}

void agent_registry_init(jvk_agent_registry_t* reg)
{
    if (reg == NULL) {
        return;
    }
    memset(reg, 0, sizeof(*reg));
    reg->next_def_id = 1;
    reg->next_task_id = 1;
    reg->stage_plan = 1;
    reg->stage_think = 2;
    reg->stage_act = 2;
    reg->stage_report = 1;
    reg->max_tasks = JVK_AGENT_MAX_TASKS;
}

void agent_set_observer(jvk_agent_registry_t* reg,
                        agent_event_fn fn, void* user)
{
    if (reg == NULL) {
        return;
    }
    reg->on_event = fn;
    reg->event_user = user;
}

void agent_emit(jvk_agent_registry_t* reg, const char* message)
{
    if (reg != NULL && reg->on_event != NULL && message != NULL) {
        reg->on_event(reg->event_user, message);
    }
}

const jvk_agent_def_t* agent_def_by_key(const jvk_agent_registry_t* reg,
                                        const char* key)
{
    if (reg == NULL || key == NULL) {
        return NULL;
    }
    for (int i = 0; i < reg->def_count; i++) {
        if (strcmp(reg->defs[i].key, key) == 0) {
            return &reg->defs[i];
        }
    }
    return NULL;
}

const jvk_agent_def_t* agent_def_by_name(const jvk_agent_registry_t* reg,
                                         const char* name)
{
    if (reg == NULL || name == NULL) {
        return NULL;
    }
    for (int i = 0; i < reg->def_count; i++) {
        if (strcmp(reg->defs[i].name, name) == 0) {
            return &reg->defs[i];
        }
    }
    return NULL;
}

const jvk_agent_def_t* agent_def_by_id(const jvk_agent_registry_t* reg,
                                       int id)
{
    if (reg == NULL) {
        return NULL;
    }
    for (int i = 0; i < reg->def_count; i++) {
        if (reg->defs[i].id == id) {
            return &reg->defs[i];
        }
    }
    return NULL;
}

int agent_running_def(const jvk_agent_registry_t* reg, int def_id)
{
    if (reg == NULL) {
        return 0;
    }
    for (int i = 0; i < JVK_AGENT_MAX_LIVE; i++) {
        if (reg->instances[i].used && reg->instances[i].def_id == def_id) {
            return reg->instances[i].pid;
        }
    }
    return 0;
}

int agent_define(jvk_agent_registry_t* reg, const jvk_agent_def_t* def)
{
    if (reg == NULL || def == NULL || def->key[0] == '\0' ||
        def->name[0] == '\0') {
        return -1;
    }
    for (int i = 0; i < reg->def_count; i++) {
        if (strcmp(reg->defs[i].key, def->key) != 0) {
            continue;
        }
        if (reg->defs[i].builtin) {
            return -1;
        }
        int id = reg->defs[i].id;
        reg->defs[i] = *def;
        reg->defs[i].id = id;
        reg->defs[i].builtin = 0;
        return id;
    }
    if (reg->def_count >= JVK_AGENT_MAX_DEFS) {
        return -1;
    }
    int idx = reg->def_count++;
    reg->defs[idx] = *def;
    reg->defs[idx].id = reg->next_def_id++;
    reg->defs[idx].builtin = def->builtin ? 1 : 0;
    if (reg->defs[idx].tool_count < 0) {
        reg->defs[idx].tool_count = 0;
    }
    if (reg->defs[idx].tool_count > JVK_AGENT_MAX_TOOLS) {
        reg->defs[idx].tool_count = JVK_AGENT_MAX_TOOLS;
    }
    return reg->defs[idx].id;
}

int agent_remove(jvk_agent_registry_t* reg, int def_id)
{
    if (reg == NULL || agent_running_def(reg, def_id)) {
        return 0;
    }
    for (int i = 0; i < reg->def_count; i++) {
        if (reg->defs[i].id != def_id) {
            continue;
        }
        if (reg->defs[i].builtin) {
            return 0;
        }
        for (int j = i; j < reg->def_count - 1; j++) {
            reg->defs[j] = reg->defs[j + 1];
        }
        reg->def_count--;
        memset(&reg->defs[reg->def_count], 0, sizeof(jvk_agent_def_t));
        return 1;
    }
    return 0;
}

static void set_builtin(jvk_agent_def_t* d, const char* key, const char* name,
                        const char* role, const char* personality,
                        const char* color, const char* icon,
                        int priority, int mem_pages)
{
    memset(d, 0, sizeof(*d));
    d->builtin = 1;
    copy_text(d->key, sizeof(d->key), key);
    copy_text(d->name, sizeof(d->name), name);
    copy_text(d->role, sizeof(d->role), role);
    copy_text(d->personality, sizeof(d->personality), personality);
    copy_text(d->color, sizeof(d->color), color);
    copy_text(d->icon, sizeof(d->icon), icon);
    d->priority = priority;
    d->mem_pages = mem_pages;
}

static void add_tool(jvk_agent_def_t* d, const char* tool)
{
    if (d->tool_count >= JVK_AGENT_MAX_TOOLS) {
        return;
    }
    copy_text(d->tools[d->tool_count], JVK_AGENT_TOOL_LEN, tool);
    d->tool_count++;
}

int agent_seed_builtins(jvk_agent_registry_t* reg)
{
    if (reg == NULL) {
        return 0;
    }
    struct {
        const char* key;
        const char* name;
        const char* role;
        const char* personality;
        const char* color;
        const char* icon;
        int priority;
        int mem_pages;
        const char* tool0;
        const char* tool1;
        const char* tool2;
    } builtins[] = {
        {"finance", "Finance Agent", "Budget planning and expense analysis",
         "Analytical, precise", "#22d3ee", "wallet", 6, 4,
         "query_budget", "compute_total", NULL},
        {"coding", "Coding Agent", "Specification review, code generation and test runs",
         "Methodical, test-first", "#34d399", "code", 7, 6,
         "read_spec", "generate_code", "run_tests"},
        {"research", "Research Agent", "Search the virtual filesystem and summarise findings",
         "Curious, evidence-driven", "#a78bfa", "search", 5, 4,
         "search_files", "read_file", "summarize"},
        {"writing", "Writing Agent", "Draft and rewrite documents for the agent workspace",
         "Concise, expressive", "#fbbf24", "pen", 4, 3,
         "write_draft", "rewrite", NULL},
        {"hr", "HR Agent", "Onboarding checklists and policy lookups",
         "Supportive, organised", "#f472b6", "users", 4, 3,
         "onboarding_checklist", "policy_lookup", NULL},
        {"legal", "Legal Agent", "Clause extraction and compliance checks",
         "Cautious, exact", "#f87171", "scale", 5, 3,
         "extract_clauses", "compliance_check", NULL},
        {"marketing", "Marketing Agent", "Campaign ideation and report generation",
         "Creative, energetic", "#fb923c", "megaphone", 3, 3,
         "brainstorm", "campaign_report", NULL},
        {"travel", "Travel Agent", "Itinerary planning and booking checks",
         "Practical, upbeat", "#38bdf8", "plane", 3, 3,
         "plan_itinerary", "booking_check", NULL},
        {"health", "Health Agent", "Wellness checklists and reminders",
         "Calm, encouraging", "#a3e635", "heart", 4, 3,
         "wellness_checklist", "reminder", NULL},
    };
    int count = 0;
    for (int i = 0; i < (int)(sizeof(builtins) / sizeof(builtins[0])); i++) {
        jvk_agent_def_t d;
        set_builtin(&d, builtins[i].key, builtins[i].name, builtins[i].role,
                    builtins[i].personality, builtins[i].color,
                    builtins[i].icon, builtins[i].priority,
                    builtins[i].mem_pages);
        add_tool(&d, builtins[i].tool0);
        if (builtins[i].tool1 != NULL) {
            add_tool(&d, builtins[i].tool1);
        }
        if (builtins[i].tool2 != NULL) {
            add_tool(&d, builtins[i].tool2);
        }
        if (agent_define(reg, &d) > 0) {
            count++;
        }
    }
    return count;
}
