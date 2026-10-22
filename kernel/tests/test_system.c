#include <stddef.h>
/*
 * kernel/tests/test_system.c
 *
 * JARVIS OS — M0..M6 integration smoke test.
 *
 * Drives the kernel through its public ABI (jvk_init/jvk_command/
 * jvk_tick/jvk_snapshot/jvk_logs) and checks the cross-subsystem work
 * finished for the MVP:
 *   M2  machine / motherboard model (config-driven board + POST)
 *   M3a scheduler metrics (CPU utilization, avg wait, avg turnaround)
 *   M3b per-agent memory budget (enforced, logged, reported)
 *   M4  context-switch interrupt (raised on a real pid change)
 *   M5  filesystem present in the snapshot
 *   M6  device config (enabled/disabled, IRQ wiring) + disk write buffer
 *
 * Run from kernel/:  make test
 */

#include <stdio.h>
#include <string.h>

#include "abi.h"

static int g_failures = 0;

static void expect(int cond, const char* what)
{
    if (!cond) {
        printf("FAIL: %s\n", what);
        g_failures++;
    }
}

static int has(const char* haystack, const char* needle)
{
    return haystack != NULL && needle != NULL &&
           strstr(haystack, needle) != NULL;
}

static void test_machine_and_policies(void)
{
    const char* cfg =
        "{\"boot\":true,"
        "\"machine\":{\"board\":\"JVK-TEST\",\"chipset\":\"VC-T\","
        "\"busSpeedMHz\":900,\"bootDevice\":\"/dev/vdisk0\"},"
        "\"memory\":{\"totalPages\":64,\"allocator\":\"first_fit\","
        "\"replacement\":\"clock\",\"swapEnabled\":true,\"swapSlots\":64,"
        "\"agentBudgetPages\":2,\"pageTableStrategy\":\"per-agent\","
        "\"presentBit\":true,\"referencedBit\":true,\"dirtyBit\":false},"
        "\"devices\":{\"mouse\":{\"enabled\":false},"
        "\"printer\":{\"irq\":\"timer\"}}}";
    expect(has(jvk_init(cfg), "ok"), "kernel boots with config");

    const char* snap = jvk_snapshot();
    expect(has(snap, "\"machine\""), "snapshot exposes machine");
    expect(has(snap, "JVK-TEST"), "board name comes from config");
    expect(has(snap, "VC-T"), "chipset comes from config");
    expect(has(snap, "\"bus_speed_mhz\":900"), "bus speed comes from config");
    expect(has(snap, "\"post_ok\":true"), "POST succeeded");
    expect(has(snap, "\"page_table_strategy\":\"per-agent\""),
           "paging policy surfaced");
    expect(has(snap, "\"agent_budget_pages\":2"), "budget config surfaced");
}

static void test_device_config(void)
{
    const char* snap = jvk_snapshot();
    /* config disabled the mouse -> it must not be CONNECTED */
    expect(has(snap, "\"key\":\"mouse\"") && has(snap, "DISABLED"),
           "mouse disabled by config");
    /* config moved the printer onto the timer line */
    expect(has(snap, "\"name\":\"Printer\"") && has(snap, "\"irq\":\"timer\""),
           "printer IRQ rewired by config");
    expect(has(snap, "\"name\":\"Keyboard\"") && has(snap, "\"irq\":\"keyboard\""),
           "keyboard keeps its default line");
}

static void test_disk_write_buffer(void)
{
    expect(has(jvk_command("{\"action\":\"device_write\",\"device\":\"disk\","
                           "\"data\":\"block-one\"}"), "\"ok\":true"),
           "disk accepts write 1");
    expect(has(jvk_command("{\"action\":\"device_write\",\"device\":\"disk\","
                           "\"data\":\"block-two\"}"), "\"ok\":true"),
           "disk accepts write 2");
    const char* snap = jvk_snapshot();
    expect(has(snap, "\"buffered\":2"), "both writes sit in the buffer");
    expect(has(jvk_command("{\"action\":\"device_flush\",\"device\":\"disk\"}"),
               "\"flushed\":2"),
           "flush commits the buffered batch");
    expect(has(jvk_snapshot(), "\"buffered\":0"), "buffer empty after flush");
}

static void test_budget_enforcement(void)
{
    expect(has(jvk_command("{\"action\":\"create_process\",\"name\":\"agent-a\","
                           "\"mem_budget\":2}"), "\"ok\":true"),
           "process with 2-page budget created");
    expect(has(jvk_command("{\"action\":\"mem_alloc\",\"pid\":1,\"pages\":2}"),
               "\"ok\":true"),
           "allocation within budget succeeds");
    const char* denied =
        jvk_command("{\"action\":\"mem_alloc\",\"pid\":1,\"pages\":1}");
    expect(has(denied, "\"ok\":false") && has(denied, "budget"),
           "allocation beyond budget is denied");
    expect(has(jvk_logs(0), "MEM_BUDGET_DENIED"), "denial is logged");
    const char* snap = jvk_snapshot();
    expect(has(snap, "\"mem_budget_pages\":2"), "budget shown per process");
    expect(has(snap, "\"used\":2") && has(snap, "\"budget\":2"),
           "page table reports used/budget");
}

static void test_metrics_and_context_switch(void)
{
    expect(has(jvk_command("{\"action\":\"create_process\",\"name\":\"agent-b\"}"),
               "\"ok\":true"),
           "second process created");
    for (int i = 0; i < 6; i++) {
        jvk_tick();
    }
    const char* snap = jvk_snapshot();
    expect(has(snap, "\"cpu_utilization\""), "cpu utilization metric present");
    expect(has(snap, "\"avg_wait\""), "average wait metric present");
    expect(has(snap, "\"avg_turnaround\""), "average turnaround metric present");
    const char* logs = jvk_logs(0);
    expect(has(logs, "IRQ_HANDLED irq=context_switch"),
           "context-switch interrupt serviced");
    expect(has(snap, "\"context_switch_irqs\":"), "switch IRQs counted");

    expect(has(jvk_command("{\"action\":\"kill_process\",\"pid\":2}"),
               "\"ok\":true"),
           "process killed");
    expect(has(jvk_command("{\"action\":\"list_scheduler\"}"),
               "\"completed\":1"),
           "completed count updated after kill");
}

int main(void)
{
    test_machine_and_policies();
    test_device_config();
    test_disk_write_buffer();
    test_budget_enforcement();
    test_metrics_and_context_switch();

    if (g_failures == 0) {
        printf("test_system: all checks passed\n");
        return 0;
    }
    printf("test_system: %d failure(s)\n", g_failures);
    return 1;
}
