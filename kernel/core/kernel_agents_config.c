#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "kernel_agents_internal.h"

static const cJSON* get_item(const cJSON* obj, const char* a, const char* b,
                             const char* c)
{
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, a);
    if (!cJSON_IsString(item) && !cJSON_IsNumber(item) && b != NULL) {
        item = cJSON_GetObjectItemCaseSensitive(obj, b);
    }
    if (!cJSON_IsString(item) && !cJSON_IsNumber(item) && c != NULL) {
        item = cJSON_GetObjectItemCaseSensitive(obj, c);
    }
    return item;
}

static void copy_text(char* dst, int cap, const char* src)
{
    if (cap > 0) {
        snprintf(dst, (size_t)cap, "%s", src == NULL ? "" : src);
    }
}

static int get_int(const cJSON* obj, const char* a, const char* b,
                   int fallback)
{
    const cJSON* item = get_item(obj, a, b, NULL);
    return cJSON_IsNumber(item) ? (int)item->valuedouble : fallback;
}

void kagent_slug(const char* name, char* out, int cap)
{
    int pos = 0;
    int dash = 0;
    if (cap <= 0) {
        return;
    }
    for (int i = 0; name != NULL && name[i] != '\0' && pos < cap - 1; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (isalnum(ch)) {
            out[pos++] = (char)tolower(ch);
            dash = 0;
        } else if (!dash && pos > 0) {
            out[pos++] = '-';
            dash = 1;
        }
    }
    while (pos > 0 && out[pos - 1] == '-') {
        pos--;
    }
    out[pos] = '\0';
    if (pos == 0) {
        copy_text(out, cap, "agent");
    }
}

static void parse_tools(const cJSON* item, jvk_agent_def_t* def)
{
    def->tool_count = 0;
    if (cJSON_IsArray(item)) {
        const cJSON* tool = NULL;
        cJSON_ArrayForEach(tool, item) {
            if (cJSON_IsString(tool) && def->tool_count < JVK_AGENT_MAX_TOOLS) {
                copy_text(def->tools[def->tool_count], JVK_AGENT_TOOL_LEN,
                          tool->valuestring);
                def->tool_count++;
            }
        }
        return;
    }
    if (!cJSON_IsString(item)) {
        return;
    }
    const char* text = item->valuestring;
    char token[JVK_AGENT_TOOL_LEN];
    int pos = 0;
    for (int i = 0;; i++) {
        char ch = text[i];
        if (ch == ',' || ch == ' ' || ch == '\0') {
            token[pos] = '\0';
            if (pos > 0 && def->tool_count < JVK_AGENT_MAX_TOOLS) {
                copy_text(def->tools[def->tool_count], JVK_AGENT_TOOL_LEN,
                          token);
                def->tool_count++;
            }
            pos = 0;
            if (ch == '\0') {
                break;
            }
        } else if (pos < JVK_AGENT_TOOL_LEN - 1) {
            token[pos++] = ch;
        }
    }
}

int kagent_parse_def(const cJSON* item, jvk_agent_def_t* def)
{
    if (!cJSON_IsObject(item) || def == NULL) {
        return 0;
    }
    memset(def, 0, sizeof(*def));
    const cJSON* name = get_item(item, "name", NULL, NULL);
    const cJSON* key = get_item(item, "key", "id", "slug");
    if (!cJSON_IsString(name) || name->valuestring[0] == '\0') {
        return 0;
    }
    copy_text(def->name, sizeof(def->name), name->valuestring);
    if (cJSON_IsString(key) && key->valuestring[0] != '\0') {
        copy_text(def->key, sizeof(def->key), key->valuestring);
    } else {
        kagent_slug(def->name, def->key, sizeof(def->key));
    }
    const cJSON* role = get_item(item, "role", "description", "system_prompt");
    const cJSON* pers = get_item(item, "personality", "style", NULL);
    const cJSON* color = get_item(item, "color", "colour", NULL);
    const cJSON* icon = get_item(item, "icon", NULL, NULL);
    copy_text(def->role, sizeof(def->role),
              cJSON_IsString(role) ? role->valuestring : "Simulated AI agent");
    copy_text(def->personality, sizeof(def->personality),
              cJSON_IsString(pers) ? pers->valuestring : "Balanced");
    copy_text(def->color, sizeof(def->color),
              cJSON_IsString(color) ? color->valuestring : "#22d3ee");
    copy_text(def->icon, sizeof(def->icon),
              cJSON_IsString(icon) ? icon->valuestring : "bot");
    parse_tools(get_item(item, "tools", NULL, NULL), def);
    def->priority = get_int(item, "priority", NULL, 4);
    if (def->priority < 0) def->priority = 0;
    if (def->priority > 10) def->priority = 10;
    def->mem_pages = get_int(item, "mem_pages", "memPages", 3);
    if (def->mem_pages < 1) def->mem_pages = 1;
    if (def->mem_pages > 32) def->mem_pages = 32;
    const cJSON* builtin = get_item(item, "builtin", NULL, NULL);
    def->builtin = cJSON_IsBool(builtin) && cJSON_IsTrue(builtin) ? 1 : 0;
    return 1;
}

void kagent_apply_config(jvk_agent_registry_t* reg, const cJSON* cfg)
{
    if (!cJSON_IsObject(cfg)) {
        return;
    }
    const cJSON* max_tasks = get_item(cfg, "maxTasks", "max_tasks", NULL);
    if (cJSON_IsNumber(max_tasks)) {
        int n = (int)max_tasks->valuedouble;
        if (n < 1) n = 1;
        if (n > JVK_AGENT_MAX_TASKS) n = JVK_AGENT_MAX_TASKS;
        reg->max_tasks = n;
    }
    const cJSON* stages = get_item(cfg, "stageTicks", "stage_ticks", NULL);
    if (cJSON_IsObject(stages)) {
        reg->stage_plan = get_int(stages, "plan", NULL, reg->stage_plan);
        reg->stage_think = get_int(stages, "think", NULL, reg->stage_think);
        reg->stage_act = get_int(stages, "act", NULL, reg->stage_act);
        reg->stage_report = get_int(stages, "report", NULL, reg->stage_report);
    }
    if (reg->stage_plan < 1) reg->stage_plan = 1;
    if (reg->stage_think < 1) reg->stage_think = 1;
    if (reg->stage_act < 1) reg->stage_act = 1;
    if (reg->stage_report < 1) reg->stage_report = 1;
}

int kagent_resolve_def(const jvk_agent_registry_t* reg, const cJSON* req)
{
    const cJSON* id = get_item(req, "id", "def_id", NULL);
    if (cJSON_IsNumber(id)) {
        const jvk_agent_def_t* def = agent_def_by_id(reg, (int)id->valuedouble);
        return def != NULL ? def->id : -1;
    }
    const cJSON* key = get_item(req, "key", "agent", "name");
    if (cJSON_IsString(key)) {
        const jvk_agent_def_t* def = agent_def_by_key(reg, key->valuestring);
        if (def == NULL) {
            def = agent_def_by_name(reg, key->valuestring);
        }
        return def != NULL ? def->id : -1;
    }
    return -1;
}

int kagent_resolve_pid(const jvk_agent_registry_t* reg, const cJSON* req)
{
    const cJSON* pid = get_item(req, "pid", NULL, NULL);
    if (cJSON_IsNumber(pid) && pid->valuedouble > 0) {
        const jvk_agent_instance_t* inst =
            agent_instance_by_pid((jvk_agent_registry_t*)reg,
                                  (int)pid->valuedouble);
        return inst != NULL ? inst->pid : -1;
    }
    int def_id = kagent_resolve_def(reg, req);
    if (def_id > 0) {
        return agent_running_def(reg, def_id);
    }
    return -1;
}
