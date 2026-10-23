#ifndef JARVIS_AGENT_TYPES_H
#define JARVIS_AGENT_TYPES_H

#define JVK_AGENT_KEY_LEN      24
#define JVK_AGENT_NAME_LEN     32
#define JVK_AGENT_ROLE_LEN     96
#define JVK_AGENT_PERS_LEN     48
#define JVK_AGENT_COLOR_LEN    16
#define JVK_AGENT_ICON_LEN     8
#define JVK_AGENT_TOOL_LEN     24
#define JVK_AGENT_MAX_TOOLS    4
#define JVK_AGENT_MAX_DEFS     24
#define JVK_AGENT_MAX_LIVE     8
#define JVK_AGENT_MAX_TASKS    8
#define JVK_AGENT_TITLE_LEN    64
#define JVK_AGENT_INPUT_LEN    96
#define JVK_AGENT_PLAN_LEN     256
#define JVK_AGENT_TEXT_LEN     192
#define JVK_AGENT_REPORT_LEN   512

typedef enum {
    JVK_TASK_QUEUED = 0,
    JVK_TASK_PLANNING,
    JVK_TASK_THINKING,
    JVK_TASK_ACTING,
    JVK_TASK_REPORTING,
    JVK_TASK_DONE,
    JVK_TASK_FAILED,
    JVK_TASK_CANCELLED,
} jvk_task_stage_t;

typedef enum {
    JVK_AGENT_EFFECT_NONE = 0,
    JVK_AGENT_EFFECT_TOOL,
    JVK_AGENT_EFFECT_REPORT,
} jvk_agent_effect_kind_t;

typedef struct {
    int  id;
    int  builtin;
    char key[JVK_AGENT_KEY_LEN];
    char name[JVK_AGENT_NAME_LEN];
    char role[JVK_AGENT_ROLE_LEN];
    char personality[JVK_AGENT_PERS_LEN];
    char color[JVK_AGENT_COLOR_LEN];
    char icon[JVK_AGENT_ICON_LEN];
    char tools[JVK_AGENT_MAX_TOOLS][JVK_AGENT_TOOL_LEN];
    int  tool_count;
    int  priority;
    int  mem_pages;
} jvk_agent_def_t;

typedef struct {
    int               id;
    int               def_id;
    int               pid;
    jvk_task_stage_t  stage;
    int               stage_ticks;
    int               progress;
    char              title[JVK_AGENT_TITLE_LEN];
    char              input[JVK_AGENT_INPUT_LEN];
    char              tool[JVK_AGENT_TOOL_LEN];
    char              plan[JVK_AGENT_PLAN_LEN];
    char              thought[JVK_AGENT_TEXT_LEN];
    char              action[JVK_AGENT_TEXT_LEN];
    char              result[JVK_AGENT_TEXT_LEN];
    char              report[JVK_AGENT_REPORT_LEN];
    int               writes;
    int               reads;
    unsigned long     created_ticks;
    unsigned long     started_ticks;
    unsigned long     completed_ticks;
} jvk_agent_task_t;

typedef struct {
    int               used;
    int               pid;
    int               def_id;
    char              key[JVK_AGENT_KEY_LEN];
    char              name[JVK_AGENT_NAME_LEN];
    int               priority;
    int               mem_pages;
    int               memory_growth;
    int               cpu_ticks;
    int               current_task;
    int               task_count;
    jvk_agent_task_t  tasks[JVK_AGENT_MAX_TASKS];
    long              tasks_queued;
    long              tasks_done;
    long              tasks_failed;
    unsigned long     launched_ticks;
} jvk_agent_instance_t;

typedef struct {
    int  kind;
    int  pid;
    int  def_id;
    int  task_id;
    char key[JVK_AGENT_KEY_LEN];
    char agent[JVK_AGENT_NAME_LEN];
    char tool[JVK_AGENT_TOOL_LEN];
    char input[JVK_AGENT_INPUT_LEN];
    char report[JVK_AGENT_REPORT_LEN];
} jvk_agent_effect_t;

typedef void (*agent_event_fn)(void* user, const char* message);

typedef struct {
    jvk_agent_def_t      defs[JVK_AGENT_MAX_DEFS];
    int                  def_count;
    int                  next_def_id;
    jvk_agent_instance_t instances[JVK_AGENT_MAX_LIVE];
    int                  live_count;
    int                  next_task_id;
    int                  stage_plan;
    int                  stage_think;
    int                  stage_act;
    int                  stage_report;
    int                  max_tasks;
    long                 stat_launched;
    long                 stat_tasks_queued;
    long                 stat_tasks_done;
    long                 stat_tasks_failed;
    long                 stat_reports;
    agent_event_fn       on_event;
    void*                event_user;
} jvk_agent_registry_t;

#endif
