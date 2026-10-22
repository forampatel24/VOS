/*
 * kernel/tests/test_shell.c
 *
 * JARVIS OS — shell parser and command smoke test (M8).
 * Run from kernel/:  make test
 */

#include <stdio.h>
#include <string.h>

#include "shell/shell_parse.h"
#include "abi.h"

static int g_failures = 0;
static char g_out[4096];

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

static int has(const char* text, const char* needle)
{
    return text != NULL && strstr(text, needle) != NULL;
}

static void test_tokenizer(void)
{
    char args[SH_MAX_ARGS][SH_ARG_LEN];
    int n = sh_tokenize("  echo   \"hello world\"  ok ", args, SH_MAX_ARGS);
    expect(n == 3, "three tokens");
    expect(strcmp(args[0], "echo") == 0, "first token");
    expect(strcmp(args[1], "hello world") == 0, "quoted token keeps space");
    expect(strcmp(args[2], "ok") == 0, "trailing token");
    expect(sh_tokenize("echo \"open", args, SH_MAX_ARGS) == -1,
           "unterminated quote rejected");
    expect(sh_tokenize("", args, SH_MAX_ARGS) == 0, "empty line");
    expect(sh_tokenize(NULL, args, SH_MAX_ARGS) == -1, "null line");
    expect(sh_tokenize("a b c", args, 2) == -1, "too many args");
}

static void test_paths(void)
{
    char out[SH_PATH_MAX];
    expect(sh_join_path(out, sizeof(out), "/", "readme.txt") == 1 &&
           strcmp(out, "/readme.txt") == 0, "absolute from root");
    expect(sh_join_path(out, sizeof(out), "/home", "docs") == 1 &&
           strcmp(out, "/home/docs") == 0, "relative join");
    expect(sh_join_path(out, sizeof(out), "/home/docs", "../notes") == 1 &&
           strcmp(out, "/home/notes") == 0, "parent collapse");
    expect(sh_join_path(out, sizeof(out), "/home", "/etc") == 1 &&
           strcmp(out, "/etc") == 0, "absolute overrides cwd");
    expect(sh_join_path(out, sizeof(out), "/home", "../../..") == 1 &&
           strcmp(out, "/") == 0, "cannot escape root");
    expect(sh_join_path(out, sizeof(out), "/home", ".") == 1 &&
           strcmp(out, "/home") == 0, "dot stays");
    int value = 0;
    expect(sh_parse_int("42", &value) && value == 42, "parse int");
    expect(!sh_parse_int("4x", &value), "reject trailing junk");
    expect(sh_is("mem", "memory", "mem"), "alias match");
    expect(!sh_is("memo", "memory", "mem"), "no prefix match");
}

static void test_shell_commands(void)
{
    expect(strcmp(jvk_init("{\"boot\":true}"), "ok") == 0, "boot");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"echo \\\"a b\\\"\"}"),
               "\"output\":\"a b"), "echo preserves quotes");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"pwd\"}"),
               "jarvis@vos:/$"), "prompt in reply");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"cd /\"}"),
               "\"cwd\":\"/\""), "cd root");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"mkdir /t\"}"),
               "created directory /t"), "mkdir");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"touch /t/f\"}"),
               "created /t/f"), "touch");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"ls /t\"}"),
               "f"), "ls shows file");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"cat /t/f\"}"),
               "\"ok\":true"), "cat empty file");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"rm /t/f\"}"),
               "deleted /t/f"), "rm");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"badcmd\"}"),
               "command not found"), "unknown command text");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"badcmd\"}"),
               "\"ok\":false"), "unknown command status");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"clear\"}"),
               "\"clear\":true"), "clear flag");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"exit\"}"),
               "\"exit\":true"), "exit flag");
}

static void test_shell_ipc_commands(void)
{
    expect(strcmp(jvk_init("{\"boot\":true}"), "ok") == 0, "boot ipc");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"send agent-bus hi\"}"),
               "\"ok\":true"), "send command");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"recv agent-bus\"}"),
               "hi"), "recv command");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"ipc\"}"),
               "agent-bus"), "ipc command lists seeded queue");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"devices\"}"),
               "DEVICE"), "devices command");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"sched\"}"),
               "algo="), "sched command");
}

int main(void)
{
    test_tokenizer();
    test_paths();
    test_shell_commands();
    test_shell_ipc_commands();

    if (g_failures == 0) {
        printf("test_shell: PASS\n");
        return 0;
    }
    printf("test_shell: %d failure(s)\n", g_failures);
    return 1;
}
