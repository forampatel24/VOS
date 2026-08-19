/*
 * kernel/tests/test_process.c
 *
 * JARVIS OS — process manager smoke test (M3a).
 *
 * Exercises the lifecycle (create / suspend / resume / kill), the PID
 * generator, queue membership and the NASM-backed context-switch swap.
 * Run from the kernel/ directory:
 *
 *     mingw32-make test
 */

#include <stdio.h>
#include <stdint.h>

#include "process/context_switch.h"
#include "process/process_manager.h"

static int g_failures = 0;

static void expect(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

static void expect_pid(const jvk_queue_t* q, int pid, const char* what)
{
    expect(queue_contains(q, pid), what);
}

int main(void)
{
    jvk_process_manager_t pm;
    pm_init(&pm);

    /* ---- PID generation ---- */
    int pid_a = 0, pid_b = 0;
    expect(pm_create(&pm, "alpha", 0, 0, &pid_a) == 1, "create alpha");
    expect(pm_create(&pm, "beta", 0, 0, &pid_b) == 1, "create beta");
    expect(pid_a == 1 && pid_b == 2, "PIDs are monotonic from 1");

    /* ---- queues after create ---- */
    expect(pm.count == 2, "two live processes");
    expect_pid(&pm.ready, pid_a, "alpha in ready");
    expect_pid(&pm.ready, pid_b, "beta in ready");
    expect(pm.suspended.count == 0, "nothing suspended yet");

    /* ---- context-switch blob swap (NASM-backed) ---- */
    jvk_ctx_t ctx_x = {{0}};
    jvk_ctx_t ctx_y = {{0}};
    ctx_x.q[0] = 11;
    ctx_x.q[8] = 100; /* PC slot */
    ctx_y.q[0] = 22;
    ctx_y.q[8] = 200;
    ctx_swap(&ctx_x, &ctx_y);
    expect(ctx_x.q[0] == 22 && ctx_x.q[8] == 200, "ctx a took b values");
    expect(ctx_y.q[0] == 11 && ctx_y.q[8] == 100, "ctx b took a values");

    /* ---- suspend / resume lifecycle ---- */
    expect(pm_suspend(&pm, pid_a) == 1, "suspend alpha");
    expect_pid(&pm.suspended, pid_a, "alpha in suspended");
    expect(pm.ready.count == 1, "alpha left ready");
    expect(pm_suspend(&pm, pid_a) == 0, "suspend again rejected");
    expect(pm_resume(&pm, pid_a) == 1, "resume alpha");
    expect_pid(&pm.ready, pid_a, "alpha back in ready");
    expect(pm.suspended.count == 0, "suspended queue empty");
    expect(pm_resume(&pm, pid_b) == 0, "resume running/ready rejected");

    /* ---- round-robin next ready ----
       alpha was resumed last, so it sits at the back of the ready queue:
       beta (2) rotates out first, then alpha (1). */
    expect(pm_next_ready(&pm) == pid_b, "next ready rotates to beta");
    expect(pm_next_ready(&pm) == pid_a, "next ready rotates to alpha");
    expect(pm.switches == 2, "two context switches counted");

    /* ---- kill ---- */
    expect(pm_kill(&pm, pid_b) == 1, "kill beta");
    expect(pm.count == 1, "one live process after kill");
    expect_pid(&pm.terminated, pid_b, "beta in terminated");
    expect(pm_get(&pm, pid_b) == NULL, "beta slot freed");
    expect(pm_kill(&pm, pid_b) == 0, "killing again rejected");
    expect(pm_kill(&pm, 999) == 0, "killing unknown pid rejected");

    /* ---- full-table rejection ---- */
    jvk_process_manager_t full;
    pm_init(&full);
    int ok = 1;
    for (int i = 0; i < JVK_MAX_PROCS; i++) {
        int pid = 0;
        if (!pm_create(&full, "fill", 0, 0, &pid)) {
            ok = 0;
            break;
        }
    }
    expect(ok == 1 && full.count == JVK_MAX_PROCS, "table fills to cap");
    int extra = 0;
    expect(pm_create(&full, "overflow", 0, 0, &extra) == 0, "create rejected when full");

    if (g_failures == 0) {
        printf("PASS: process manager smoke test\n");
        return 0;
    }
    printf("process manager smoke test FAILED (%d assertion(s))\n", g_failures);
    return 1;
}