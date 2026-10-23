#include <stdio.h>
#include <string.h>

#include "abi.h"

static int g_failures = 0;
static char g_out[8192];
static char g_snap[131072];

static void expect(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

static const char* cmd(const char* json)
{
    snprintf(g_out, sizeof(g_out), "%s", jvk_command(json));
    return g_out;
}

static const char* snap(void)
{
    snprintf(g_snap, sizeof(g_snap), "%s", jvk_snapshot());
    return g_snap;
}

static int has(const char* text, const char* needle)
{
    return text != NULL && strstr(text, needle) != NULL;
}

static int number_for(const char* json, const char* key)
{
    char needle[48];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char* at = json != NULL ? strstr(json, needle) : NULL;
    int value = -1;
    if (at != NULL) {
        if (sscanf(at + strlen(needle), "%d", &value) != 1) {
            value = -1;
        }
    }
    return value;
}

int main(void)
{
    expect(strcmp(jvk_init("{\"boot\":true,\"agents\":{\"stageTicks\":"
                           "{\"plan\":1,\"think\":1,\"act\":1,\"report\":1}}}"),
                  "ok") == 0, "boot");
    const char* s = snap();
    expect(has(s, "\"definitions\":["), "agents snapshot present");
    expect(has(s, "\"key\":\"finance\""), "finance builtin seeded");
    expect(has(s, "\"key\":\"coding\""), "coding builtin seeded");
    expect(has(s, "\"key\":\"research\""), "research builtin seeded");
    expect(has(s, "\"key\":\"health\""), "health builtin seeded");
    expect(has(cmd("{\"action\":\"list_agents\"}"), "\"agents\""),
           "list_agents answers");

    const char* r = cmd("{\"action\":\"agent_launch\",\"agent\":\"finance\"}");
    expect(has(r, "\"ok\":true"), "launch finance");
    int pid = number_for(r, "pid");
    expect(pid > 0, "finance got a pid");
    expect(has(snap(), "\"activity\":\"IDLE\""), "agent idle before tasks");

    const char* task = cmd("{\"action\":\"agent_task\",\"agent\":\"finance\","
                           "\"title\":\"Monthly budget review\","
                           "\"input\":\"budget 1200 300 45\"}");
    expect(has(task, "\"ok\":true"), "queue finance task");
    int task_id = number_for(task, "task_id");
    expect(task_id > 0, "task id assigned");

    expect(has(cmd("{\"action\":\"tick\",\"count\":30}"), "\"ok\":true"),
           "advance ticks");
    s = snap();
    expect(has(s, "\"stage\":\"DONE\""), "task reached DONE");
    expect(has(s, "budget envelope 1200"), "tool result recorded");
    expect(has(s, "\"reports\":1"), "report counted");

    char path[128];
    snprintf(path, sizeof(path), "{\"action\":\"fs_read\",\"path\":"
             "\"/Agents/finance/report-%d.md\"}", task_id);
    expect(has(cmd(path), "Finance Agent"), "report file written");
    s = snap();
    expect(has(s, "\"cpu_ticks\":"), "agent cpu accounted");
    const char* instances = strstr(s, "\"instances\"");
    int mem = instances != NULL ? number_for(instances, "mem_pages") : -1;
    expect(mem > 4, "agent grew past initial memory budget");

    const char* custom =
        "{\"action\":\"agent_define\",\"key\":\"audit\","
        "\"name\":\"Audit Agent\",\"role\":\"Check ledgers\","
        "\"personality\":\"Skeptical\",\"color\":\"#ffffff\","
        "\"icon\":\"shield\",\"tools\":[\"read_file\",\"summarize\"],"
        "\"priority\":7,\"mem_pages\":2}";
    r = cmd(custom);
    expect(has(r, "\"ok\":true"), "define custom agent");
    int def_id = number_for(r, "id");
    expect(def_id > 0, "custom definition id");

    r = cmd("{\"action\":\"agent_launch\",\"key\":\"audit\"}");
    expect(has(r, "\"ok\":true"), "launch custom agent");
    int audit_pid = number_for(r, "pid");
    expect(audit_pid > 0 && audit_pid != pid, "custom agent pid");

    expect(has(cmd("{\"action\":\"agent_remove\",\"key\":\"audit\"}"),
               "\"ok\":false"), "cannot remove a running agent");
    expect(has(cmd("{\"action\":\"agent_kill\",\"pid\":99}"), "\"ok\":false"),
           "agent_kill rejects unknown pid");
    expect(has(cmd("{\"action\":\"agent_kill\",\"pid\":98}"), "\"ok\":false"),
           "agent_kill rejects non-agent pid");

    char kill[64];
    snprintf(kill, sizeof(kill), "{\"action\":\"agent_kill\",\"pid\":%d}",
             audit_pid);
    expect(has(cmd(kill), "\"ok\":true"), "kill custom agent");
    expect(has(snap(), "\"key\":\"audit\""), "custom def survives kill");
    expect(has(cmd("{\"action\":\"agent_remove\",\"key\":\"audit\"}"),
               "\"ok\":true"), "remove idle custom agent");
    expect(!has(snap(), "\"key\":\"audit\""), "custom def removed");

    expect(has(cmd("{\"action\":\"kill_process\",\"pid\":1}"), "\"ok\":true"),
           "kill finance process");
    expect(has(cmd("{\"action\":\"agent_task\",\"agent\":\"finance\","
                   "\"title\":\"after kill\"}"), "\"ok\":false"),
           "task on dead agent rejected");

    if (g_failures == 0) {
        printf("PASS: test_agents\n");
    } else {
        printf("FAILURES: %d\n", g_failures);
    }
    return g_failures == 0 ? 0 : 1;
}
