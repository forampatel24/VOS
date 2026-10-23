#ifndef JARVIS_KERNEL_AGENTS_INTERNAL_H
#define JARVIS_KERNEL_AGENTS_INTERNAL_H

#include "kernel_agents.h"

int  kagent_parse_def(const cJSON* item, jvk_agent_def_t* def);
void kagent_slug(const char* name, char* out, int cap);
void kagent_apply_config(jvk_agent_registry_t* reg, const cJSON* cfg);
int  kagent_resolve_def(const jvk_agent_registry_t* reg, const cJSON* req);
int  kagent_resolve_pid(const jvk_agent_registry_t* reg, const cJSON* req);

#endif
