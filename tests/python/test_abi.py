"""
tests/python/test_abi.py

M1 smoke test: the kernel ABI round-trip through ctypes and through the
FastAPI bridge. Run from the project root:

    python -m pytest tests/python/test_abi.py -v
"""

from __future__ import annotations

from fastapi.testclient import TestClient

from backend.main import app
from backend.services.kernel_loader import (
    jvk_command,
    jvk_init,
    jvk_logs,
    jvk_shutdown,
    jvk_snapshot,
    jvk_tick,
)


def test_jvk_init_returns_ok() -> None:
    assert jvk_init({"boot": True}) == "ok"


def test_jvk_command_ping_is_real() -> None:
    result = jvk_command({"action": "ping"})
    assert result["ok"] is True
    assert result["kernel"] == "c-native"


def test_jvk_command_echo() -> None:
    result = jvk_command({"action": "echo", "message": "hello"})
    assert result["ok"] is True
    assert result["echo"] == "hello"


def test_jvk_command_unknown_action_returns_error() -> None:
    result = jvk_command({"action": "does_not_exist"})
    assert result["ok"] is False
    assert "unknown action" in result["error"]


def test_jvk_tick_advances_snapshot() -> None:
    before = jvk_snapshot()["uptime_ticks"]
    jvk_tick()
    jvk_tick()
    after = jvk_snapshot()["uptime_ticks"]
    assert after == before + 2


def test_jvk_logs_are_incremental() -> None:
    jvk_command({"action": "ping"})
    first = jvk_logs(0)["logs"]
    assert len(first) > 0
    # logs from `since` return only newer entries
    later = jvk_logs(len(first))["logs"]
    assert later == []


# ---- M2 CPU & clock ---------------------------------------------------

SUM_PROGRAM = [
    0x1000, 0,      # MOV R0, 0
    0x1100, 10,     # MOV R1, 10
    0x2010,         # loop: ADD R0, R1
    0x1200, 1,      # MOV R2, 1
    0x3120,         # SUB R1, R2
    0x6000, 12,     # JZ  halt
    0x5000, 4,      # JMP loop
    0x8000,         # halt: HALT
]


def test_cpu_load_program() -> None:
    jvk_init({"boot": True})
    result = jvk_command({"action": "cpu_load_program", "program": SUM_PROGRAM})
    assert result["ok"] is True
    assert result["words"] == len(SUM_PROGRAM)


def test_cpu_program_runs_to_halt() -> None:
    jvk_init({"boot": True})
    jvk_command({"action": "cpu_load_program", "program": SUM_PROGRAM})
    for _ in range(200):
        snap = jvk_snapshot()
        if snap["cpu"]["halted"]:
            break
        jvk_tick()
    snap = jvk_snapshot()
    assert snap["cpu"]["halted"] is True
    assert snap["cpu"]["registers"]["R0"] == 55
    assert snap["cpu"]["pc"] == 12


def test_cpu_reset_clears_registers() -> None:
    jvk_init({"boot": True})
    jvk_command({"action": "cpu_load_program", "program": SUM_PROGRAM})
    for _ in range(10):
        jvk_tick()
    jvk_command({"action": "cpu_reset"})
    snap = jvk_snapshot()
    assert snap["cpu"]["pc"] == 0
    assert snap["cpu"]["halted"] is False
    assert snap["cpu"]["registers"]["R0"] == 0


def test_cpu_step_executes_one_instruction() -> None:
    jvk_init({"boot": True})
    jvk_command({"action": "cpu_load_program", "program": [0x1000, 0, 0x1100, 10, 0x8000]})
    r = jvk_command({"action": "cpu_step"})
    assert r["ok"] is True
    assert r["pc"] == 2
    assert r["R0"] == 0
    r = jvk_command({"action": "cpu_step"})
    assert r["pc"] == 4
    assert r["R1"] == 10
    r = jvk_command({"action": "cpu_step"})
    assert r["halted"] is True


def test_cpu_cmp_sets_zero_flag() -> None:
    jvk_init({"boot": True})
    # MOV R0,5 ; MOV R1,5 ; CMP R0,R1 ; HALT
    jvk_command({"action": "cpu_load_program", "program": [0x1000, 5, 0x1100, 5, 0x4010, 0x8000]})
    for _ in range(10):
        jvk_tick()
    snap = jvk_snapshot()
    assert snap["cpu"]["halted"] is True
    assert snap["cpu"]["flags"]["Z"] is True


def test_clock_config_is_parsed() -> None:
    jvk_init({"boot": True, "clock": {"speed_hz": 2000, "quantum": 3}})
    snap = jvk_snapshot()
    assert snap["clock"]["speed_hz"] == 2000
    assert snap["clock"]["quantum"] == 3


# ---- M3 process manager ------------------------------------------------

def test_create_process_returns_pid() -> None:
    jvk_init({"boot": True})
    r = jvk_command({"action": "create_process", "name": "agent_finance"})
    assert r["ok"] is True
    assert r["pid"] == 1
    assert r["name"] == "agent_finance"


def test_create_process_increments_pid() -> None:
    jvk_init({"boot": True})
    a = jvk_command({"action": "create_process", "name": "alpha"})
    b = jvk_command({"action": "create_process", "name": "beta"})
    assert a["pid"] == 1
    assert b["pid"] == 2


def test_process_lifecycle_suspend_resume() -> None:
    jvk_init({"boot": True})
    r = jvk_command({"action": "create_process", "name": "alpha"})
    pid = r["pid"]

    s = jvk_command({"action": "suspend_process", "pid": pid})
    assert s["ok"] is True
    snap = jvk_snapshot()
    assert snap["queues"]["suspended"] == [pid]
    assert snap["queues"]["ready"] == []

    s = jvk_command({"action": "resume_process", "pid": pid})
    assert s["ok"] is True
    snap = jvk_snapshot()
    assert snap["queues"]["ready"] == [pid]
    assert snap["queues"]["suspended"] == []


def test_process_kill_moves_to_terminated() -> None:
    jvk_init({"boot": True})
    r = jvk_command({"action": "create_process", "name": "alpha"})
    pid = r["pid"]
    r = jvk_command({"action": "kill_process", "pid": pid})
    assert r["ok"] is True
    snap = jvk_snapshot()
    assert snap["queues"]["terminated"] == [pid]
    assert snap["queues"]["ready"] == []
    assert snap["processes"] == 0


def test_process_bad_pid_rejected() -> None:
    jvk_init({"boot": True})
    assert jvk_command({"action": "kill_process", "pid": 999})["ok"] is False
    assert jvk_command({"action": "suspend_process", "pid": 999})["ok"] is False
    assert jvk_command({"action": "resume_process", "pid": 999})["ok"] is False


def test_list_processes_reports_state() -> None:
    jvk_init({"boot": True})
    a = jvk_command({"action": "create_process", "name": "alpha"})
    b = jvk_command({"action": "create_process", "name": "beta"})
    jvk_command({"action": "suspend_process", "pid": a["pid"]})

    r = jvk_command({"action": "list_processes"})
    assert r["ok"] is True
    states = {p["pid"]: p["state"] for p in r["processes"]}
    assert states[a["pid"]] == "SUSPENDED"
    assert states[b["pid"]] == "READY"
    assert r["queues"]["suspended"] == [a["pid"]]


def test_process_events_are_logged() -> None:
    jvk_init({"boot": True})
    r = jvk_command({"action": "create_process", "name": "alpha"})
    pid = r["pid"]
    jvk_command({"action": "suspend_process", "pid": pid})
    jvk_command({"action": "resume_process", "pid": pid})
    jvk_command({"action": "kill_process", "pid": pid})

    logs = " | ".join(e["message"] for e in jvk_logs(0)["logs"])
    assert "PROCESS_CREATED" in logs
    assert "PROCESS_SUSPENDED" in logs
    assert "PROCESS_RESUMED" in logs
    assert "PROCESS_KILLED" in logs


# ---- M3b memory manager -------------------------------------------------

MEM_CFG = {"boot": True, "memory": {
    "totalPages": 16, "allocator": "first_fit",
    "replacement": "clock", "swapEnabled": True, "swapSlots": 8,
}}


def test_mem_alloc_maps_frames() -> None:
    jvk_init(MEM_CFG)
    jvk_command({"action": "create_process", "name": "p1"})
    r = jvk_command({"action": "mem_alloc", "pid": 1, "pages": 2})
    assert r["ok"] is True
    assert r["base_vpage"] == 0
    assert r["contiguous"] is True
    assert r["frames"] == [0, 1]
    snap = jvk_snapshot()
    assert snap["memory"]["stats"]["frames_used"] == 2
    assert snap["memory"]["stats"]["pages_mapped"] == 2
    assert snap["memory"]["frame_map"][:2] == [1, 1]


def test_mem_read_write_roundtrip() -> None:
    jvk_init(MEM_CFG)
    jvk_command({"action": "create_process", "name": "p1"})
    jvk_command({"action": "mem_alloc", "pid": 1, "pages": 2})
    w = jvk_command({"action": "mem_write", "pid": 1, "addr": 19, "value": 77})
    assert w["ok"] is True
    assert w["vpage"] == 1 and w["offset"] == 3
    r = jvk_command({"action": "mem_read", "pid": 1, "addr": 19})
    assert r["ok"] is True and r["value"] == 77


def test_mem_segv_on_unmapped_access() -> None:
    jvk_init(MEM_CFG)
    jvk_command({"action": "create_process", "name": "p1"})
    jvk_command({"action": "mem_alloc", "pid": 1, "pages": 1})
    r = jvk_command({"action": "mem_read", "pid": 1, "addr": 999})
    assert r["ok"] is False and "segmentation" in r["error"]
    assert jvk_snapshot()["memory"]["stats"]["segfaults"] == 1


def test_mem_free_releases_frames() -> None:
    jvk_init(MEM_CFG)
    jvk_command({"action": "create_process", "name": "p1"})
    jvk_command({"action": "mem_alloc", "pid": 1, "pages": 4})
    f = jvk_command({"action": "mem_free", "pid": 1})
    assert f["ok"] is True and f["pages_freed"] == 4
    stats = jvk_snapshot()["memory"]["stats"]
    assert stats["frames_used"] == 0
    assert stats["frames_free"] == 16
    bad = jvk_command({"action": "mem_free", "pid": 1})
    assert bad["ok"] is False  # address space already gone


def test_kill_process_releases_memory() -> None:
    jvk_init(MEM_CFG)
    p = jvk_command({"action": "create_process", "name": "agent_x"})
    pid = p["pid"]
    jvk_command({"action": "mem_alloc", "pid": pid, "pages": 3})
    k = jvk_command({"action": "kill_process", "pid": pid})
    assert k["ok"] is True
    assert k["memory_pages_freed"] == 3
    assert jvk_snapshot()["memory"]["stats"]["frames_used"] == 0


def test_mem_pressure_swaps_and_restores_value() -> None:
    small = {"boot": True, "memory": {
        "totalPages": 4, "allocator": "first_fit",
        "replacement": "clock", "swapEnabled": True, "swapSlots": 8,
    }}
    jvk_init(small)
    for n in ["p1", "p2", "p3"]:
        jvk_command({"action": "create_process", "name": n})
    jvk_command({"action": "mem_alloc", "pid": 1, "pages": 2})
    jvk_command({"action": "mem_alloc", "pid": 2, "pages": 2})
    jvk_command({"action": "mem_write", "pid": 1, "addr": 0, "value": 777})

    a = jvk_command({"action": "mem_alloc", "pid": 3, "pages": 1})
    assert a["ok"] is True  # forced an eviction

    mem = jvk_snapshot()["memory"]
    assert mem["stats"]["swap_outs"] >= 1
    assert mem["swap_used"] >= 1

    r = jvk_command({"action": "mem_read", "pid": 1, "addr": 0})
    assert r["ok"] is True and r["value"] == 777  # survived round-trip
    logs = " | ".join(e["message"] for e in jvk_logs(0)["logs"])
    assert "PAGE_FAULT" in logs and "SWAP_IN" in logs


def test_mem_oom_without_swap() -> None:
    tiny = {"boot": True, "memory": {
        "totalPages": 4, "allocator": "first_fit",
        "replacement": "clock", "swapEnabled": False,
    }}
    jvk_init(tiny)
    jvk_command({"action": "create_process", "name": "p1"})
    jvk_command({"action": "create_process", "name": "p2"})
    jvk_command({"action": "mem_alloc", "pid": 1, "pages": 4})
    r = jvk_command({"action": "mem_alloc", "pid": 2, "pages": 1})
    assert r["ok"] is False and "out of memory" in r["error"]


def test_mem_config_switches_strategies() -> None:
    jvk_init(MEM_CFG)
    c = jvk_command({"action": "mem_config",
                     "allocator": "best_fit", "replacement": "lru"})
    assert c["ok"] is True
    assert c["allocator"] == "best_fit"
    assert c["replacement"] == "lru"
    bad = jvk_command({"action": "mem_config", "allocator": "turbo"})
    assert bad["ok"] is False


def test_mem_alloc_rejects_unknown_pid() -> None:
    jvk_init(MEM_CFG)
    jvk_command({"action": "create_process", "name": "p1"})
    r = jvk_command({"action": "mem_alloc", "pid": 99, "pages": 1})
    assert r["ok"] is False
    assert "no such process" in r["error"]


def test_memory_events_are_logged() -> None:
    jvk_init(MEM_CFG)
    jvk_command({"action": "create_process", "name": "p1"})
    jvk_command({"action": "mem_alloc", "pid": 1, "pages": 1})
    jvk_command({"action": "mem_write", "pid": 1, "addr": 0, "value": 5})
    jvk_command({"action": "mem_free", "pid": 1})
    logs = " | ".join(e["message"] for e in jvk_logs(0)["logs"])
    assert "MEMORY_ALLOCATED pid=1 pages=1" in logs
    assert "MEMORY_FREED pid=1" in logs


# ---- M4 interrupt controller & error manager -----------------------------

def test_trigger_interrupt_enqueues() -> None:
    jvk_init({"boot": True})
    r = jvk_command({"action": "trigger_interrupt", "irq": "timer", "source": "test"})
    assert r["ok"] is True
    assert r["irq"] == "timer"
    snap = jvk_snapshot()
    assert snap["interrupts"]["pending"] >= 1 or snap["interrupts"]["handled"] >= 1  # may be serviced on next tick
    # list
    lst = jvk_command({"action": "list_interrupts"})
    assert lst["ok"] is True
    assert "interrupts" in lst


def test_interrupt_priority_ordering() -> None:
    jvk_init({"boot": True})
    # Enqueue low prio first, then high — high must be queued ahead (priority order)
    jvk_command({"action": "trigger_interrupt", "irq": "software", "source": "low"})
    jvk_command({"action": "trigger_interrupt", "irq": "page_fault", "source": "high"})
    snap = jvk_snapshot()
    assert snap["interrupts"]["queue"][0]["irq"] == "page_fault"  # priority 1 beats 5
    # Strict drain: one tick handles *all* pending, then scheduler runs
    before = snap["interrupts"]["handled"]
    jvk_tick()
    after = jvk_snapshot()["interrupts"]["handled"]
    assert after == before + 2
    assert jvk_snapshot()["interrupts"]["pending"] == 0
    logs = " | ".join(e["message"] for e in jvk_logs(0)["logs"])
    assert "IRQ_ENQUEUED" in logs
    assert "IRQ_HANDLED" in logs
    # bad irq rejected
    bad = jvk_command({"action": "trigger_interrupt", "irq": "bogus"})
    assert bad["ok"] is False


def test_page_fault_generates_interrupt() -> None:
    cfg = {"boot": True, "memory": {"totalPages": 4, "allocator": "first_fit", "replacement": "clock", "swapEnabled": True, "swapSlots": 8}}
    jvk_init(cfg)
    for n in ["p1", "p2", "p3"]:
        jvk_command({"action": "create_process", "name": n})
    jvk_command({"action": "mem_alloc", "pid": 1, "pages": 2})
    jvk_command({"action": "mem_alloc", "pid": 2, "pages": 2})
    jvk_command({"action": "mem_write", "pid": 1, "addr": 0, "value": 123})
    jvk_command({"action": "mem_alloc", "pid": 3, "pages": 1})  # evicts one page
    # Touch the evicted page to fault — trampoline enqueues page_fault IRQ
    jvk_command({"action": "mem_read", "pid": 1, "addr": 0})
    logs = " | ".join(e["message"] for e in jvk_logs(0)["logs"])
    assert "PAGE_FAULT" in logs
    before = jvk_snapshot()["interrupts"]["handled"]
    jvk_tick()  # services the fault IRQ
    assert jvk_snapshot()["interrupts"]["handled"] == before + 1


def test_panic_flag() -> None:
    jvk_init({"boot": True})
    snap = jvk_snapshot()
    assert snap["interrupts"]["panic"] is False
    r = jvk_command({"action": "panic", "reason": "test panic"})
    assert r["ok"] is True
    snap = jvk_snapshot()
    assert snap["interrupts"]["panic"] is True
    assert "test panic" in snap["interrupts"]["panic_reason"]
    assert snap["error_manager"]["counts"]["system"] >= 1
    r = jvk_command({"action": "clear_panic"})
    assert r["ok"] is True
    assert jvk_snapshot()["interrupts"]["panic"] is False
    # interrupt queue full drops
    for _ in range(35):
        jvk_command({"action": "trigger_interrupt", "irq": "software", "source": "fill"})
    snap = jvk_snapshot()
    assert snap["interrupts"]["dropped"] >= 1


# ---- M4.5 priority / SJF / FCFS -----------------------------------------

def test_scheduler_priority_picks_highest() -> None:
    jvk_init({"boot": True})
    a = jvk_command({"action": "create_process", "name": "low", "priority": 1})
    b = jvk_command({"action": "create_process", "name": "high", "priority": 10})
    c = jvk_command({"action": "create_process", "name": "mid", "priority": 5})
    jvk_command({"action": "scheduler_config", "algo": "priority"})
    snap = jvk_snapshot()
    assert snap["scheduler"]["algo"] == "priority"
    assert snap["scheduler"]["next"] == b["pid"]  # highest prio
    jvk_tick()
    # Priority stays with high until it is killed/suspended
    assert jvk_snapshot()["scheduler"]["current"] == b["pid"]
    # FCFS picks earliest arrival (lowest created_ticks)
    jvk_init({"boot": True})
    jvk_command({"action": "create_process", "name": "first", "priority": 1})
    jvk_tick()
    jvk_command({"action": "create_process", "name": "second", "priority": 10})
    jvk_command({"action": "scheduler_config", "algo": "fcfs"})
    assert jvk_snapshot()["scheduler"]["next"] == 1  # first arrived


def test_scheduler_sjf_picks_shortest_burst() -> None:
    jvk_init({"boot": True})
    a = jvk_command({"action": "create_process", "name": "long", "burst_time": 10})
    b = jvk_command({"action": "create_process", "name": "short", "burst_time": 2})
    jvk_command({"action": "scheduler_config", "algo": "sjf"})
    assert jvk_snapshot()["scheduler"]["next"] == b["pid"]
    # RR still works and stores burst
    jvk_command({"action": "scheduler_config", "algo": "round_robin"})
    snap = jvk_snapshot()
    assert snap["scheduler"]["algo"] == "round_robin"
    assert snap["process_list"][0]["burst_time"] == 10
    assert snap["process_list"][1]["burst_time"] == 2


def test_scheduler_config_rejects_unknown() -> None:
    jvk_init({"boot": True})
    bad = jvk_command({"action": "scheduler_config", "algo": "bogus"})
    assert bad["ok"] is False
    assert "unknown algo" in bad["error"]


def test_fastapi_health_endpoint() -> None:
    with TestClient(app) as client:
        resp = client.get("/health")
        assert resp.status_code == 200
        body = resp.json()
        assert body["ok"] is True
        assert body["kernel"]["booted"] is True


def test_fastapi_command_endpoint() -> None:
    with TestClient(app) as client:
        resp = client.post("/api/command", json={"action": "ping"})
        assert resp.status_code == 200
        body = resp.json()
        assert body["ok"] is True
        assert body["kernel"] == "c-native"


def test_fastapi_tick_endpoint() -> None:
    with TestClient(app) as client:
        resp = client.get("/api/tick")
        assert resp.status_code == 200
        assert resp.json()["ok"] is True


def test_jvk_shutdown() -> None:
    jvk_shutdown()
    snap = jvk_snapshot()
    assert snap["shutdown"] is True