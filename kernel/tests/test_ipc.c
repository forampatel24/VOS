/*
 * kernel/tests/test_ipc.c
 *
 * JARVIS OS — IPC smoke test (M7).
 *
 * Drives the public JSON ABI end to end: seeded objects, queues and
 * blocking/wakeup, shared memory, pipes, mutex/semaphore handoff,
 * deadlock detection and recovery (which goes through kill_process).
 * Run from kernel/:  make test
 */

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

static void copy_reply(const char* reply)
{
    snprintf(g_out, sizeof(g_out), "%s", reply != NULL ? reply : "");
}

static const char* cmd(const char* json)
{
    copy_reply(jvk_command(json));
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

static int boot(void)
{
    return strcmp(jvk_init("{\"boot\":true}"), "ok") == 0;
}

static int make_proc(const char* name)
{
    char json[128];
    snprintf(json, sizeof(json),
             "{\"action\":\"create_process\",\"name\":\"%s\",\"burst\":5}", name);
    return has(jvk_command(json), "\"ok\":true");
}

static void test_seeded_objects(void)
{
    expect(boot(), "boot");
    const char* s = snap();
    expect(has(s, "\"ipc\""), "snapshot has ipc");
    expect(has(s, "agent-bus"), "seeded queue");
    expect(has(s, "shared-board"), "seeded segment");
    expect(has(s, "console-pipe"), "seeded pipe");
    expect(has(s, "log-lock"), "seeded mutex");
    expect(has(s, "worker-slots"), "seeded semaphore");
    expect(has(s, "\"shell\""), "snapshot has shell");
}

static void test_queue_roundtrip(void)
{
    expect(boot(), "boot queue");
    expect(has(cmd("{\"action\":\"ipc_queue_create\",\"queue\":\"tq\",\"capacity\":4}"),
               "\"ok\":true"), "create queue");
    expect(has(cmd("{\"action\":\"ipc_queue_send\",\"queue\":\"tq\",\"data\":\"hello\"}"),
               "\"ok\":true"), "send message");
    expect(has(cmd("{\"action\":\"ipc_queue_recv\",\"queue\":\"tq\"}"),
               "\"data\":\"hello\""), "receive message");
    expect(has(cmd("{\"action\":\"ipc_queue_recv\",\"queue\":\"tq\",\"nonblock\":true}"),
               "\"ok\":false"), "empty queue errors");
    expect(has(snap(), "\"messages_sent\":1"), "sent counter");
}

static void test_queue_block_wake(void)
{
    expect(boot(), "boot queue block");
    expect(make_proc("A") && make_proc("B"), "two processes");
    expect(has(cmd("{\"action\":\"ipc_queue_create\",\"queue\":\"bq\",\"capacity\":1}"),
               "\"ok\":true"), "create capacity-1 queue");
    expect(has(cmd("{\"action\":\"ipc_queue_send\",\"queue\":\"bq\",\"pid\":1,\"data\":\"one\"}"),
               "\"ok\":true"), "first send fits");
    expect(has(cmd("{\"action\":\"ipc_queue_send\",\"queue\":\"bq\",\"pid\":2,\"data\":\"two\"}"),
               "\"blocked\":true"), "second send blocks");
    expect(has(snap(), "\"state\":\"WAITING\""), "sender is WAITING");
    expect(has(cmd("{\"action\":\"ipc_queue_recv\",\"queue\":\"bq\",\"pid\":1}"),
               "\"data\":\"one\""), "receiver gets first");
    expect(has(cmd("{\"action\":\"ipc_queue_recv\",\"queue\":\"bq\",\"pid\":1}"),
               "\"data\":\"two\""), "blocked sender auto-committed");
    const char* s = snap();
    expect(has(s, "\"woken\":1"), "wake accounted");
    expect(has(s, "\"state\":\"READY\""), "sender READY again");
}

static void test_shm_and_pipe(void)
{
    expect(boot(), "boot shm/pipe");
    expect(make_proc("A") && make_proc("B"), "two processes");
    expect(has(cmd("{\"action\":\"ipc_shm_create\",\"segment\":\"ss\",\"size\":64}"),
               "\"ok\":true"), "create segment");
    expect(has(cmd("{\"action\":\"ipc_shm_attach\",\"segment\":\"ss\",\"pid\":2}"),
               "\"ok\":true"), "attach");
    expect(has(cmd("{\"action\":\"ipc_shm_write\",\"segment\":\"ss\",\"pid\":2,\"data\":\"abc\"}"),
               "\"bytes\":3"), "write segment");
    expect(has(cmd("{\"action\":\"ipc_shm_read\",\"segment\":\"ss\",\"pid\":2}"),
               "\"data\":\"abc\""), "read segment");
    expect(has(cmd("{\"action\":\"ipc_shm_detach\",\"segment\":\"ss\",\"pid\":2}"),
               "\"ok\":true"), "detach");

    expect(has(cmd("{\"action\":\"ipc_pipe_create\",\"pipe\":\"pp\",\"pid\":1,\"capacity\":64}"),
               "\"ok\":true"), "create pipe");
    expect(has(cmd("{\"action\":\"ipc_pipe_open\",\"pipe\":\"pp\",\"pid\":2,\"mode\":\"r\"}"),
               "\"ok\":true"), "open reader");
    expect(has(cmd("{\"action\":\"ipc_pipe_write\",\"pipe\":\"pp\",\"pid\":1,\"data\":\"ping\"}"),
               "\"bytes\":4"), "pipe write");
    expect(has(cmd("{\"action\":\"ipc_pipe_read\",\"pipe\":\"pp\",\"pid\":2,\"max\":16}"),
               "\"data\":\"ping\""), "pipe read");
}

static void test_sync_handoff(void)
{
    expect(boot(), "boot sync");
    expect(make_proc("A") && make_proc("B"), "two processes");
    expect(has(cmd("{\"action\":\"ipc_mutex_create\",\"mutex\":\"mx\"}"),
               "\"ok\":true"), "create mutex");
    expect(has(cmd("{\"action\":\"ipc_mutex_lock\",\"mutex\":\"mx\",\"pid\":1}"),
               "\"ok\":true"), "first lock");
    expect(has(cmd("{\"action\":\"ipc_mutex_lock\",\"mutex\":\"mx\",\"pid\":2}"),
               "\"blocked\":true"), "contended lock blocks");
    expect(has(cmd("{\"action\":\"ipc_mutex_unlock\",\"mutex\":\"mx\",\"pid\":1}"),
               "\"ok\":true"), "unlock hands off");
    expect(has(snap(), "\"woken\":1"), "mutex waiter woken");

    expect(has(cmd("{\"action\":\"ipc_sem_create\",\"semaphore\":\"sm\",\"value\":1,\"max\":2}"),
               "\"ok\":true"), "create semaphore");
    expect(has(cmd("{\"action\":\"ipc_sem_wait\",\"semaphore\":\"sm\",\"pid\":1}"),
               "\"ok\":true"), "sem wait takes permit");
    expect(has(cmd("{\"action\":\"ipc_sem_wait\",\"semaphore\":\"sm\",\"pid\":2}"),
               "\"blocked\":true"), "second wait blocks");
    expect(has(cmd("{\"action\":\"ipc_sem_post\",\"semaphore\":\"sm\",\"pid\":1}"),
               "\"ok\":true"), "post hands permit over");
    expect(has(snap(), "\"sem_waits\""), "sem stats present");
}

static void test_deadlock_and_recover(void)
{
    expect(boot(), "boot deadlock");
    expect(make_proc("A") && make_proc("B"), "two processes");
    expect(has(cmd("{\"action\":\"ipc_mutex_create\",\"mutex\":\"d1\"}"),
               "\"ok\":true"), "create d1");
    expect(has(cmd("{\"action\":\"ipc_mutex_create\",\"mutex\":\"d2\"}"),
               "\"ok\":true"), "create d2");
    expect(has(cmd("{\"action\":\"ipc_mutex_lock\",\"mutex\":\"d1\",\"pid\":1}"),
               "\"ok\":true"), "p1 locks d1");
    expect(has(cmd("{\"action\":\"ipc_mutex_lock\",\"mutex\":\"d2\",\"pid\":2}"),
               "\"ok\":true"), "p2 locks d2");
    expect(has(cmd("{\"action\":\"ipc_mutex_lock\",\"mutex\":\"d2\",\"pid\":1}"),
               "\"blocked\":true"), "p1 blocks on d2");
    expect(has(cmd("{\"action\":\"ipc_mutex_lock\",\"mutex\":\"d1\",\"pid\":2}"),
               "\"blocked\":true"), "p2 blocks on d1");
    expect(has(cmd("{\"action\":\"ipc_deadlock\"}"),
               "\"deadlock\":true"), "cycle detected");
    expect(has(snap(), "\"active\":true"), "snapshot deadlock active");
    expect(has(cmd("{\"action\":\"ipc_recover\",\"pid\":2}"),
               "\"recovered\":2"), "recover kills victim");
    expect(has(cmd("{\"action\":\"ipc_deadlock\"}"),
               "\"deadlock\":false"), "cycle cleared");
}

static void test_shell_core(void)
{
    expect(boot(), "boot shell");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"help\"}"),
               "JARVIS shell commands"), "help");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"version\"}"),
               "JARVIS OS"), "version");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"ps\"}"),
               "PID"), "ps header");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"pwd\"}"),
               "\"output\":\"/"), "pwd");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"memory\"}"),
               "frames"), "memory");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"ipc\"}"),
               "agent-bus"), "ipc listing");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"nonsense\"}"),
               "\"ok\":false"), "unknown command rejected");
}

static void test_shell_fs(void)
{
    expect(boot(), "boot shell fs");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"mkdir /sh\"}"),
               "\"ok\":true"), "mkdir");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"write-file /sh/a.txt hello world\"}"),
               "wrote 11 byte"), "write-file");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"cat /sh/a.txt\"}"),
               "hello world"), "cat");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"ls /sh\"}"),
               "a.txt"), "ls");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"cp /sh/a.txt /sh/b.txt\"}"),
               "\"ok\":true"), "cp");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"mv /sh/b.txt /sh/c.txt\"}"),
               "\"ok\":true"), "mv");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"chmod /sh/c.txt 644\"}"),
               "\"ok\":true"), "chmod");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"rm /sh/c.txt\"}"),
               "\"ok\":true"), "rm");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"cd /sh\"}"),
               "\"ok\":true"), "cd");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"pwd\"}"),
               "\"output\":\"/sh"), "pwd after cd");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"tick 2\"}"),
               "advanced 2 tick"), "tick");
    expect(has(cmd("{\"action\":\"shell_exec\",\"line\":\"shutdown\"}"),
               "system halted"), "shutdown");
    expect(has(snap(), "\"shutdown\":true"), "kernel shutdown flag");
}

int main(void)
{
    test_seeded_objects();
    test_queue_roundtrip();
    test_queue_block_wake();
    test_shm_and_pipe();
    test_sync_handoff();
    test_deadlock_and_recover();
    test_shell_core();
    test_shell_fs();

    if (g_failures == 0) {
        printf("test_ipc: PASS\n");
        return 0;
    }
    printf("test_ipc: %d failure(s)\n", g_failures);
    return 1;
}
