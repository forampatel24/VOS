/*
 * kernel/core/kernel.c
 *
 * JARVIS OS — kernel entry point and JSON ABI.
 *
 * Milestone M1 scope: a real, bootable kernel core behind the documented
 * ABI (jvk_init / jvk_command / jvk_tick / jvk_snapshot / jvk_logs /
 * jvk_shutdown / jvk_last_error). The dispatcher routes actions by name,
 * every failing command returns a JSON error object (never crashes), and
 * events are written to an in-kernel log that the bridge polls.
 *
 * All returned JSON strings live in static buffers owned by the kernel.
 * The bridge must use ctypes restype=c_void_p + string_at and never free
 * these pointers.
 */

#define JARVIS_KERNEL_BUILD

#include <stdio.h>
#include <string.h>

#include "abi.h"

/* ---- kernel state --------------------------------------------------- */

#define JVK_LOG_CAP  64
#define JVK_LOG_LEN  256

static int           g_booted    = 0;
static unsigned long g_ticks     = 0;
static int           g_shutdown  = 0;
static char          g_last_error[JVK_LOG_LEN] = "";

static char          g_logs[JVK_LOG_CAP][JVK_LOG_LEN];
static int           g_log_count = 0;

static char          g_init_buf[128];
static char          g_cmd_buf[512];
static char          g_snap_buf[512];
static char          g_log_buf[JVK_LOG_CAP * (JVK_LOG_LEN + 32)];

/* ---- logging --------------------------------------------------------- */

static void jvk_log(const char* msg)
{
    if (g_log_count >= JVK_LOG_CAP) {
        return;
    }
    snprintf(g_logs[g_log_count], JVK_LOG_LEN, "%s", msg);
    g_log_count++;
}

static void jvk_set_error(const char* msg)
{
    snprintf(g_last_error, sizeof(g_last_error), "%s", msg);
}

/* ---- minimal JSON helpers -------------------------------------------- *
 * M1 uses the smallest parser needed: read one string value by key and
 * build responses with snprintf. A full JSON parser (vendored cJSON)
 * arrives with the M2 kernel core.
 */

static const char* find_key(const char* json, const char* key)
{
    const char* p = json;
    size_t klen = strlen(key);
    while ((p = strstr(p, key)) != NULL) {
        const char* q = p + klen;
        while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') {
            q++;
        }
        if (*q == ':') {
            return q + 1;
        }
        p = q;
    }
    return NULL;
}

static int json_get_string(const char* json, const char* key,
                           char* out, size_t out_sz)
{
    const char* v = find_key(json, key);
    if (v == NULL || out_sz == 0) {
        return 0;
    }
    while (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n') {
        v++;
    }
    if (*v != '"') {
        return 0;
    }
    v++;
    size_t i = 0;
    while (*v != '\0' && *v != '"' && i + 1 < out_sz) {
        if (*v == '\\' && v[1] == '"') {
            out[i++] = '"';
            v += 2;
            continue;
        }
        out[i++] = *v++;
    }
    out[i] = '\0';
    return 1;
}

/* ---- ABI implementation --------------------------------------------- */

const char* jvk_init(const char* config_json)
{
    (void)config_json; /* configuration is parsed in later milestones */

    g_booted    = 1;
    g_ticks     = 0;
    g_shutdown  = 0;
    g_log_count = 0;
    g_last_error[0] = '\0';

    jvk_log("kernel booted");
    snprintf(g_init_buf, sizeof(g_init_buf), "ok");
    return g_init_buf;
}

const char* jvk_command(const char* action_json)
{
    char action[64] = "";

    if (action_json == NULL ||
        !json_get_string(action_json, "\"action\"", action, sizeof(action))) {
        jvk_set_error("missing action field");
        snprintf(g_cmd_buf, sizeof(g_cmd_buf),
                 "{\"ok\":false,\"error\":\"missing action field\"}");
        return g_cmd_buf;
    }

    if (strcmp(action, "ping") == 0) {
        jvk_log("command: ping");
        snprintf(g_cmd_buf, sizeof(g_cmd_buf),
                 "{\"ok\":true,\"kernel\":\"c-native\",\"pong\":true}");
        return g_cmd_buf;
    }

    if (strcmp(action, "echo") == 0) {
        char message[128] = "";
        json_get_string(action_json, "\"message\"", message, sizeof(message));
        jvk_log("command: echo");
        snprintf(g_cmd_buf, sizeof(g_cmd_buf),
                 "{\"ok\":true,\"echo\":\"%s\"}", message);
        return g_cmd_buf;
    }

    jvk_set_error("unknown action");
    jvk_log("command rejected: unknown action");
    snprintf(g_cmd_buf, sizeof(g_cmd_buf),
             "{\"ok\":false,\"error\":\"unknown action\"}");
    return g_cmd_buf;
}

void jvk_tick(void)
{
    if (!g_booted || g_shutdown) {
        return;
    }
    g_ticks++;
    if (g_ticks % 10 == 0) {
        char msg[JVK_LOG_LEN];
        snprintf(msg, sizeof(msg), "tick %lu", g_ticks);
        jvk_log(msg);
    }
}

const char* jvk_snapshot(void)
{
    snprintf(g_snap_buf, sizeof(g_snap_buf),
             "{\"booted\":%s,\"shutdown\":%s,\"uptime_ticks\":%lu,"
             "\"processes\":0,\"memory_pages\":0}",
             g_booted   ? "true" : "false",
             g_shutdown ? "true" : "false",
             g_ticks);
    return g_snap_buf;
}

const char* jvk_logs(int since)
{
    if (since < 0) {
        since = 0;
    }
    size_t off = 0;
    off += (size_t)snprintf(g_log_buf + off, sizeof(g_log_buf) - off,
                            "{\"logs\":[");
    for (int i = since; i < g_log_count; i++) {
        size_t need = (size_t)snprintf(
            g_log_buf + off, sizeof(g_log_buf) - off,
            "%s{\"index\":%d,\"message\":\"%s\"}",
            i == since ? "" : ",", i, g_logs[i]);
        if (need >= sizeof(g_log_buf) - off) {
            break;
        }
        off += need;
    }
    snprintf(g_log_buf + off, sizeof(g_log_buf) - off, "]}");
    return g_log_buf;
}

void jvk_shutdown(void)
{
    if (g_booted && !g_shutdown) {
        jvk_log("kernel shutdown");
        g_shutdown = 1;
    }
}

int jvk_last_error(char* buf, size_t n)
{
    if (buf == NULL || n == 0) {
        return 0;
    }
    snprintf(buf, n, "%s", g_last_error);
    return (int)strlen(buf);
}