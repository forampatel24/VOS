"""
tests/python/test_abi.py

M1 smoke test: the kernel ABI round-trip through ctypes and through the
FastAPI bridge. Run from the project root:

    python -m pytest tests/python/test_abi.py -v
"""

from __future__ import annotations

from fastapi.testclient import TestClient

from backend.main import app
from backend.services.config_loader import load_config
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


# ---- M5 virtual filesystem ----------------------------------------------


def test_filesystem_is_seeded() -> None:
    jvk_init({"boot": True})
    snap = jvk_snapshot()
    assert "filesystem" in snap
    fs = snap["filesystem"]
    assert fs["config"]["total_blocks"] == 1024
    assert fs["stats"]["inodes_used"] > 0
    listing = jvk_command({"action": "fs_list", "path": "/"})
    assert listing["ok"] is True
    names = {entry["name"] for entry in listing["entries"]}
    assert {"System", "Agents", "Users", "Temp", "Logs", "Swap"} <= names


def test_filesystem_config_is_parsed() -> None:
    jvk_init({"filesystem": {"blocks": 32, "blockSize": 256}})
    fs = jvk_snapshot()["filesystem"]
    assert fs["config"]["total_blocks"] == 32
    assert fs["config"]["block_size"] == 256
    assert fs["disk"]["capacity_bytes"] == 32 * 256


def test_fs_create_read_and_append() -> None:
    jvk_init({"boot": True})
    created = jvk_command({
        "action": "fs_create",
        "path": "/Temp/note.txt",
        "content": "hello",
    })
    assert created["ok"] is True

    read = jvk_command({"action": "fs_read", "path": "/Temp/note.txt"})
    assert read["ok"] is True
    assert read["content"] == "hello"
    assert read["bytes"] == 5

    appended = jvk_command({
        "action": "fs_write",
        "path": "/Temp/note.txt",
        "content": " world",
        "append": True,
    })
    assert appended["ok"] is True
    assert appended["bytes_written"] == 6

    again = jvk_command({"action": "fs_read", "path": "/Temp/note.txt"})
    assert again["content"] == "hello world"


def test_fs_mkdir_rename_and_delete() -> None:
    jvk_init({"boot": True})
    assert jvk_command({"action": "fs_mkdir", "path": "/Temp/proj"})["ok"] is True
    assert jvk_command({
        "action": "fs_create", "path": "/Temp/proj/a.txt", "content": "x",
    })["ok"] is True

    renamed = jvk_command({
        "action": "fs_rename", "path": "/Temp/proj/a.txt", "new_name": "b.txt",
    })
    assert renamed["ok"] is True
    assert jvk_command({"action": "fs_stat", "path": "/Temp/proj/b.txt"})["ok"] is True
    assert jvk_command({"action": "fs_stat", "path": "/Temp/proj/a.txt"})["ok"] is False

    deleted = jvk_command({"action": "fs_delete", "path": "/Temp/proj/b.txt"})
    assert deleted["ok"] is True
    assert jvk_command({"action": "fs_stat", "path": "/Temp/proj/b.txt"})["ok"] is False


def test_fs_copy_move_and_search() -> None:
    jvk_init({"boot": True})
    jvk_command({"action": "fs_create", "path": "/Temp/src.txt", "content": "payload"})
    copied = jvk_command({"action": "fs_copy", "src": "/Temp/src.txt", "dst": "/Temp/copy.txt"})
    assert copied["ok"] is True
    moved = jvk_command({"action": "fs_move", "src": "/Temp/copy.txt", "dst": "/Users/Admin/copy.txt"})
    assert moved["ok"] is True
    assert jvk_command({"action": "fs_stat", "path": "/Users/Admin/copy.txt"})["ok"] is True
    assert jvk_command({"action": "fs_stat", "path": "/Temp/copy.txt"})["ok"] is False

    found = jvk_command({"action": "fs_search", "pattern": "welcome"})
    assert found["ok"] is True
    paths = {entry["path"] for entry in found["entries"]}
    assert "/Users/Admin/Documents/welcome.txt" in paths


def test_fs_permissions_and_readonly() -> None:
    jvk_init({"boot": True})
    jvk_command({"action": "fs_create", "path": "/Temp/secret.txt", "content": "s"})
    assert jvk_command({
        "action": "fs_chmod", "path": "/Temp/secret.txt", "mode": "600",
    })["ok"] is True

    denied = jvk_command({"action": "fs_read", "path": "/Temp/secret.txt", "pid": 9})
    assert denied["ok"] is False
    assert denied["error"] == "permission denied"

    readonly = jvk_command({
        "action": "fs_chmod", "path": "/Temp/secret.txt", "readonly": True,
    })
    assert readonly["ok"] is True
    blocked = jvk_command({
        "action": "fs_write", "path": "/Temp/secret.txt", "content": "x",
    })
    assert blocked["ok"] is False
    assert blocked["error"] == "read-only file"


def test_fs_open_table_and_close() -> None:
    jvk_init({"boot": True})
    assert jvk_command({
        "action": "fs_create", "path": "/Temp/fd.txt", "content": "abcdef",
    })["ok"] is True
    opened = jvk_command({"action": "fs_open", "path": "/Temp/fd.txt", "mode": "r"})
    assert opened["ok"] is True
    fd = opened["fd"]
    assert fd >= 3

    read = jvk_command({"action": "fs_read", "fd": fd})
    assert read["ok"] is True
    assert read["content"] == "abcdef"

    open_files = jvk_snapshot()["filesystem"]["open_files"]
    assert any(item["fd"] == fd for item in open_files)

    assert jvk_command({"action": "fs_close", "fd": fd})["ok"] is True
    open_files = jvk_snapshot()["filesystem"]["open_files"]
    assert all(item["fd"] != fd for item in open_files)
    assert jvk_command({"action": "fs_read", "fd": fd})["ok"] is False


def test_fs_disk_accounting() -> None:
    jvk_init({"filesystem": {"blocks": 32, "blockSize": 64}})
    before = jvk_snapshot()["filesystem"]["disk"]["used_blocks"]
    payload = "A" * 200
    assert jvk_command({
        "action": "fs_create", "path": "/Temp/big.txt", "content": payload,
    })["ok"] is True
    after = jvk_snapshot()["filesystem"]["disk"]["used_blocks"]
    assert after > before
    entry = jvk_command({"action": "fs_stat", "path": "/Temp/big.txt"})["entry"]
    assert entry["block_count"] >= 4

    assert jvk_command({"action": "fs_delete", "path": "/Temp/big.txt"})["ok"] is True
    assert jvk_snapshot()["filesystem"]["disk"]["used_blocks"] == before


def test_fs_format_reseeds_tree() -> None:
    jvk_init({"boot": True})
    jvk_command({"action": "fs_create", "path": "/Temp/temp.txt", "content": "x"})
    assert jvk_command({"action": "fs_format"})["ok"] is True
    assert jvk_command({"action": "fs_stat", "path": "/Temp/temp.txt"})["ok"] is False
    assert jvk_command({
        "action": "fs_stat", "path": "/Users/Admin/Documents/welcome.txt",
    })["ok"] is True


def test_kill_process_closes_its_files() -> None:
    jvk_init({"boot": True})
    created = jvk_command({"action": "create_process", "name": "reader", "burst_time": 5})
    pid = created["pid"]
    jvk_command({"action": "fs_create", "path": "/Temp/held.txt", "content": "x"})
    opened = jvk_command({"action": "fs_open", "path": "/Temp/held.txt", "pid": pid})
    assert opened["ok"] is True

    killed = jvk_command({"action": "kill_process", "pid": pid})
    assert killed["ok"] is True
    assert killed["files_closed"] == 1
    owners = {item["owner"] for item in jvk_snapshot()["filesystem"]["open_files"]}
    assert pid not in owners


# ---- M6 devices & drivers ------------------------------------------------


def _devices() -> dict:
    return jvk_snapshot()["devices"]


def _device(key: str) -> dict:
    return next(dev for dev in _devices()["list"] if dev["key"] == key)


def test_devices_are_registered() -> None:
    jvk_init({"boot": True})
    devices = _devices()
    assert devices["count"] == 8
    keys = {dev["key"] for dev in devices["list"]}
    assert keys == {
        "cpu", "ram", "disk", "keyboard", "mouse", "display", "printer", "clock",
    }
    listing = jvk_command({"action": "list_devices"})
    assert listing["ok"] is True
    assert listing["devices"]["count"] == 8


def test_keyboard_buffer_roundtrip() -> None:
    jvk_init({"boot": True})
    assert jvk_command({
        "action": "device_write", "device": "keyboard", "data": "hello",
    })["ok"] is True
    read = jvk_command({"action": "device_read", "device": "keyboard"})
    assert read["ok"] is True
    assert read["data"] == "hello"
    assert read["bytes"] == 5
    assert jvk_command({"action": "device_read", "device": "keyboard"})["data"] == ""


def test_mouse_position() -> None:
    jvk_init({"boot": True})
    assert jvk_command({
        "action": "device_write", "device": "mouse", "data": "640,480",
    })["ok"] is True
    info = _device("mouse")["info"]
    assert info["x"] == 640 and info["y"] == 480
    jvk_command({"action": "device_write", "device": "mouse", "data": "left"})
    assert _device("mouse")["info"]["x"] == 639


def test_display_output() -> None:
    jvk_init({"boot": True})
    assert jvk_command({
        "action": "device_write", "device": "display", "data": "booting\nready",
    })["ok"] is True
    lines = _device("display")["info"]["lines"]
    assert "booting" in lines and "ready" in lines
    read = jvk_command({"action": "device_read", "device": "display"})
    assert "booting" in read["data"]


def test_printer_spooling() -> None:
    jvk_init({"boot": True})
    jvk_command({"action": "device_write", "device": "printer", "data": "job-a"})
    jvk_command({"action": "device_write", "device": "printer", "data": "job-b"})
    assert _device("printer")["info"]["queued"] == 2
    assert _device("printer")["state"] == "WAITING"

    assert jvk_command({"action": "device_interrupt", "device": "printer"})["ok"] is True
    info = _device("printer")["info"]
    assert info["queued"] == 1
    assert info["printed"] == 1

    read = jvk_command({"action": "device_read", "device": "printer"})
    assert read["data"] == "job-b"
    assert _device("printer")["state"] == "CONNECTED"


def test_clock_device_and_timer_irq() -> None:
    jvk_init({"boot": True})
    before = _device("clock")["info"]["ticks"]
    assert jvk_command({"action": "device_interrupt", "device": "clock"})["ok"] is True
    assert _device("clock")["info"]["ticks"] == before + 1
    queue = jvk_snapshot()["interrupts"]["queue"]
    assert any(entry["irq"] == "timer" for entry in queue)


def test_device_power_and_repair() -> None:
    jvk_init({"boot": True})
    off = jvk_command({
        "action": "device_set", "device": "keyboard", "power": False,
    })
    assert off["ok"] is True
    assert off["state"] == "DISABLED"
    blocked = jvk_command({
        "action": "device_write", "device": "keyboard", "data": "x",
    })
    assert blocked["ok"] is False

    repaired = jvk_command({"action": "device_repair", "device": "keyboard"})
    assert repaired["ok"] is True and repaired["state"] == "CONNECTED"
    assert jvk_command({
        "action": "device_write", "device": "keyboard", "data": "x",
    })["ok"] is True


def test_device_fault_is_recorded() -> None:
    jvk_init({"boot": True})
    faults_before = jvk_snapshot()["error_manager"]["counts"]["device"]
    fault = jvk_command({
        "action": "device_fault", "device": "printer", "reason": "paper jam",
    })
    assert fault["ok"] is True
    assert fault["state"] == "FAULT"
    assert _device("printer")["state"] == "FAULT"
    assert _devices()["stats"]["errors"] >= 1
    assert jvk_snapshot()["error_manager"]["counts"]["device"] == faults_before + 1


def test_device_state_and_unknown_device() -> None:
    jvk_init({"boot": True})
    busy = jvk_command({
        "action": "device_set", "device": "disk", "state": "busy",
    })
    assert busy["ok"] is True and busy["state"] == "BUSY"
    missing = jvk_command({"action": "device_read", "device": "floppy"})
    assert missing["ok"] is False
    assert "unknown device" in missing["error"]
    assert jvk_command({
        "action": "device_set", "device": "disk", "state": "teleporting",
    })["ok"] is False


def test_fastapi_command_accepts_flat_device_data() -> None:
    jvk_init({"boot": True})
    with TestClient(app) as client:
        resp = client.post(
            "/api/command",
            json={"action": "device_write", "device": "keyboard", "data": "typed"},
        )
        assert resp.status_code == 200
        body = resp.json()
        assert body["ok"] is True
        assert body["bytes"] == 5

        read = client.post(
            "/api/command", json={"action": "device_read", "device": "keyboard"}
        ).json()
        assert read["data"] == "typed"


def test_fastapi_command_accepts_nested_data() -> None:
    jvk_init({"boot": True})
    with TestClient(app) as client:
        resp = client.post(
            "/api/command",
            json={"action": "fs_read", "data": {"path": "/System/README.txt"}},
        )
        assert resp.status_code == 200
        body = resp.json()
        assert body["ok"] is True
        assert "virtual filesystem" in body["content"]


def test_config_loader_merges_sections() -> None:
    cfg = load_config()
    assert cfg["boot"] is True
    # memory.json + paging.json both land in the "memory" section
    assert cfg["memory"]["totalPages"] == 1024
    assert cfg["memory"]["replacement"] == "clock"  # normalised alias
    assert cfg["memory"]["pageTableStrategy"] == "per-agent"
    assert cfg["memory"]["referencedBit"] is True
    # fs.json / device.json / machine.json each feed their own kernel section
    assert cfg["filesystem"]["blocks"] == 1024
    assert cfg["devices"]["keyboard"]["irq"] == "keyboard"
    assert cfg["machine"]["busSpeedMHz"] == 800
    # documentation keys never reach the kernel
    assert "_comment" not in cfg["memory"]


def test_machine_snapshot_from_config() -> None:
    assert jvk_init(load_config()) == "ok"
    machine = jvk_snapshot()["machine"]
    assert machine["board"] == "JVK-VB1"
    assert machine["chipset"] == "JARVIS-VC1"
    assert machine["bus_speed_mhz"] == 800
    assert machine["boot_device"] == "/dev/vdisk0"
    assert machine["power_on"] is True
    assert machine["post_ok"] is True
    assert machine["ram_frames"] == 1024
    assert len(machine["slots"]) >= 2
    kinds = {slot["kind"] for slot in machine["slots"]}
    assert {"cpu", "ram"} <= kinds


def test_agent_memory_budget_enforced() -> None:
    cfg = {"boot": True, "memory": {"totalPages": 32, "agentBudgetPages": 2}}
    assert jvk_init(cfg) == "ok"
    created = jvk_command({
        "action": "create_process", "name": "budgeted", "mem_budget": 2,
    })
    assert created["ok"] is True and created["mem_budget_pages"] == 2
    pid = created["pid"]

    assert jvk_command({"action": "mem_alloc", "pid": pid, "pages": 2})["ok"] is True
    denied = jvk_command({"action": "mem_alloc", "pid": pid, "pages": 1})
    assert denied["ok"] is False
    assert "budget" in denied["error"]

    logs = " | ".join(e["message"] for e in jvk_logs(0)["logs"])
    assert "MEM_BUDGET_SET" in logs and "MEM_BUDGET_DENIED" in logs

    entry = next(
        p for p in jvk_snapshot()["process_list"] if p["pid"] == pid
    )
    assert entry["mem_budget_pages"] == 2
    assert entry["mem_pages"] == 2


def test_default_memory_budget_applies_to_agents() -> None:
    cfg = {"boot": True, "memory": {"totalPages": 16, "agentBudgetPages": 1}}
    assert jvk_init(cfg) == "ok"
    jvk_command({"action": "create_process", "name": "capped"})
    assert jvk_command({"action": "mem_alloc", "pid": 1, "pages": 1})["ok"] is True
    assert jvk_command({"action": "mem_alloc", "pid": 1, "pages": 1})["ok"] is False


def test_scheduler_metrics_reported() -> None:
    assert jvk_init({"boot": True}) == "ok"
    jvk_command({"action": "create_process", "name": "m1", "burst_time": 4})
    second = jvk_command({"action": "create_process", "name": "m2", "burst_time": 2})
    for _ in range(6):
        jvk_tick()
    sched = jvk_snapshot()["scheduler"]
    assert 0.0 < sched["cpu_utilization"] <= 1.0
    assert sched["avg_wait"] >= 0.0
    assert sched["context_switch_irqs"] >= 1

    jvk_command({"action": "kill_process", "pid": second["pid"]})
    listing = jvk_command({"action": "list_scheduler"})
    assert listing["completed"] == 1
    assert listing["avg_turnaround"] > 0


def test_context_switch_interrupt_raised() -> None:
    assert jvk_init({"boot": True}) == "ok"
    jvk_command({"action": "create_process", "name": "c1"})
    jvk_command({"action": "create_process", "name": "c2"})
    for _ in range(6):
        jvk_tick()
    logs = " | ".join(e["message"] for e in jvk_logs(0)["logs"])
    assert "irq=context_switch" in logs
    assert jvk_snapshot()["scheduler"]["context_switch_irqs"] >= 1


def test_device_config_disable_and_irq_override() -> None:
    cfg = {
        "boot": True,
        "devices": {
            "mouse": {"enabled": False},
            "printer": {"irq": "timer"},
        },
    }
    assert jvk_init(cfg) == "ok"
    assert _device("mouse")["state"] == "DISABLED"
    assert _device("printer")["irq"] == "timer"
    assert _device("keyboard")["irq"] == "keyboard"


def test_disk_write_buffer_and_flush() -> None:
    assert jvk_init({"boot": True}) == "ok"
    jvk_command({"action": "device_write", "device": "disk", "data": "blk-1"})
    jvk_command({"action": "device_write", "device": "disk", "data": "blk-2"})
    info = _device("disk")["info"]
    assert info["buffered"] == 2 and info["flushes"] == 0

    flushed = jvk_command({"action": "device_flush", "device": "disk"})
    assert flushed["ok"] is True and flushed["flushed"] == 2
    after = _device("disk")["info"]
    assert after["buffered"] == 0 and after["flushes"] == 1


def test_fastapi_config_endpoint() -> None:
    with TestClient(app) as client:
        resp = client.get("/api/config")
        assert resp.status_code == 200
        body = resp.json()
        assert body["machine"]["chipset"] == "JARVIS-VC1"
        assert body["filesystem"]["blockSize"] == 4096


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


# ---- M7 IPC -----------------------------------------------------------


def test_ipc_snapshot_has_seeded_objects() -> None:
    assert jvk_init({"boot": True}) == "ok"
    ipc = jvk_snapshot()["ipc"]
    assert ipc["config"]["queue_capacity"] == 8
    names = {q["name"] for q in ipc["queues"]}
    assert "agent-bus" in names
    assert {s["name"] for s in ipc["segments"]} == {"shared-board"}
    assert {p["name"] for p in ipc["pipes"]} == {"console-pipe"}
    assert {m["name"] for m in ipc["mutexes"]} == {"log-lock"}
    assert {s["name"] for s in ipc["semaphores"]} == {"worker-slots"}


def test_ipc_queue_roundtrip_and_errors() -> None:
    assert jvk_init({"boot": True}) == "ok"
    assert jvk_command(
        {"action": "ipc_queue_create", "queue": "q", "capacity": 2}
    )["ok"]
    assert jvk_command(
        {"action": "ipc_queue_send", "queue": "q", "data": "hello"}
    )["ok"]
    got = jvk_command({"action": "ipc_queue_recv", "queue": "q"})
    assert got["data"] == "hello" and got["bytes"] == 5
    empty = jvk_command(
        {"action": "ipc_queue_recv", "queue": "q", "nonblock": True}
    )
    assert empty["ok"] is False and "would block" in empty["error"]
    missing = jvk_command({"action": "ipc_queue_send", "queue": "nope", "data": "x"})
    assert missing["ok"] is False
    assert jvk_snapshot()["ipc"]["stats"]["messages_sent"] == 1


def test_ipc_blocked_sender_is_woken_when_slot_frees() -> None:
    assert jvk_init({"boot": True}) == "ok"
    a = jvk_command({"action": "create_process", "name": "a"})["pid"]
    b = jvk_command({"action": "create_process", "name": "b"})["pid"]
    jvk_command({"action": "ipc_queue_create", "queue": "q1", "capacity": 1})
    jvk_command({"action": "ipc_queue_send", "queue": "q1", "pid": a, "data": "one"})
    blocked = jvk_command(
        {"action": "ipc_queue_send", "queue": "q1", "pid": b, "data": "two"}
    )
    assert blocked["blocked"] is True and blocked["state"] == "WAITING"
    states = {p["pid"]: p["state"] for p in jvk_snapshot()["process_list"]}
    assert states[b] == "WAITING"

    assert jvk_command(
        {"action": "ipc_queue_recv", "queue": "q1", "pid": a}
    )["data"] == "one"
    assert jvk_command(
        {"action": "ipc_queue_recv", "queue": "q1", "pid": a}
    )["data"] == "two"
    ipc = jvk_snapshot()["ipc"]
    assert ipc["stats"]["woken"] == 1
    states = {p["pid"]: p["state"] for p in jvk_snapshot()["process_list"]}
    assert states[b] == "READY"

    # A system caller (pid 0) never blocks; a full queue is an error.
    jvk_command({"action": "ipc_queue_send", "queue": "q1", "data": "x"})
    full = jvk_command({"action": "ipc_queue_send", "queue": "q1", "data": "y"})
    assert full["ok"] is False and full["error"] == "would block"


def test_ipc_shared_memory_and_pipe() -> None:
    assert jvk_init({"boot": True}) == "ok"
    owner = jvk_command({"action": "create_process", "name": "owner"})["pid"]
    reader = jvk_command({"action": "create_process", "name": "reader"})["pid"]
    assert jvk_command(
        {"action": "ipc_shm_create", "segment": "s", "pid": owner, "size": 64}
    )["ok"]
    assert jvk_command({"action": "ipc_shm_write", "segment": "s", "pid": owner,
                        "data": "abc"})["bytes"] == 3
    assert jvk_command({"action": "ipc_shm_attach", "segment": "s", "pid": reader})["ok"]
    got = jvk_command({"action": "ipc_shm_read", "segment": "s", "pid": reader})
    assert got["data"] == "abc"
    denied = jvk_command({"action": "ipc_shm_read", "segment": "s", "pid": 999})
    assert denied["ok"] is False

    jvk_command({"action": "ipc_pipe_create", "pipe": "p", "pid": owner, "capacity": 64})
    jvk_command({"action": "ipc_pipe_open", "pipe": "p", "pid": reader, "mode": "r"})
    assert jvk_command({"action": "ipc_pipe_write", "pipe": "p", "pid": owner,
                        "data": "ping"})["bytes"] == 4
    assert jvk_command({"action": "ipc_pipe_read", "pipe": "p", "pid": reader,
                        "max": 16})["data"] == "ping"


def test_ipc_mutex_handoff_and_semaphore() -> None:
    assert jvk_init({"boot": True}) == "ok"
    a = jvk_command({"action": "create_process", "name": "a"})["pid"]
    b = jvk_command({"action": "create_process", "name": "b"})["pid"]
    jvk_command({"action": "ipc_mutex_create", "mutex": "m"})
    assert jvk_command({"action": "ipc_mutex_lock", "mutex": "m", "pid": a})["ok"]
    assert jvk_command({"action": "ipc_mutex_lock", "mutex": "m",
                        "pid": b})["blocked"] is True
    jvk_command({"action": "ipc_mutex_unlock", "mutex": "m", "pid": a})
    mutex = next(m for m in jvk_snapshot()["ipc"]["mutexes"] if m["name"] == "m")
    assert mutex["owner"] == b and mutex["contentions"] == 1
    assert jvk_snapshot()["ipc"]["stats"]["woken"] == 1

    jvk_command({"action": "ipc_sem_create", "semaphore": "s", "value": 1, "max": 2})
    jvk_command({"action": "ipc_sem_wait", "semaphore": "s", "pid": b})
    blocked = jvk_command({"action": "ipc_sem_wait", "semaphore": "s", "pid": a})
    assert blocked["blocked"] is True
    jvk_command({"action": "ipc_sem_post", "semaphore": "s", "pid": b})
    sem = next(s for s in jvk_snapshot()["ipc"]["semaphores"] if s["name"] == "s")
    assert sem["value"] == 0 and sem["holders"] == [a]


def test_ipc_deadlock_detection_and_recovery() -> None:
    assert jvk_init({"boot": True}) == "ok"
    a = jvk_command({"action": "create_process", "name": "a"})["pid"]
    b = jvk_command({"action": "create_process", "name": "b"})["pid"]
    jvk_command({"action": "ipc_mutex_create", "mutex": "d1"})
    jvk_command({"action": "ipc_mutex_create", "mutex": "d2"})
    jvk_command({"action": "ipc_mutex_lock", "mutex": "d1", "pid": a})
    jvk_command({"action": "ipc_mutex_lock", "mutex": "d2", "pid": b})
    assert jvk_command({"action": "ipc_mutex_lock", "mutex": "d2",
                        "pid": a})["blocked"] is True
    assert jvk_command({"action": "ipc_mutex_lock", "mutex": "d1",
                        "pid": b})["blocked"] is True

    graph = jvk_command({"action": "ipc_deadlock"})
    assert graph["deadlock"] is True and graph["cycle_count"] >= 1
    assert {a, b} <= set(graph["cycles"][0])
    snap = jvk_snapshot()["ipc"]
    assert snap["wait_graph"]["active"] is True
    assert snap["stats"]["deadlocks"] == 1

    recovered = jvk_command({"action": "ipc_recover", "pid": b})
    assert recovered["recovered"] == b
    pids = {p["pid"] for p in jvk_snapshot()["process_list"]}
    assert b not in pids and a in pids
    assert jvk_command({"action": "ipc_deadlock"})["deadlock"] is False


def test_ipc_errors_are_categorised() -> None:
    assert jvk_init({"boot": True}) == "ok"
    jvk_command({"action": "ipc_queue_recv", "queue": "agent-bus", "nonblock": True})
    listing = jvk_command({"action": "list_interrupts"})
    assert listing["error_manager"]["counts"]["ipc"] >= 1
    bad = jvk_command({"action": "ipc_shm_write", "segment": "shared-board",
                       "pid": 77, "data": "x"})
    assert bad["ok"] is False and "no such process" in bad["error"]


# ---- M8 shell ---------------------------------------------------------


def test_shell_core_commands() -> None:
    assert jvk_init({"boot": True}) == "ok"
    help_out = jvk_command({"action": "shell_exec", "line": "help"})
    assert help_out["ok"] is True and "JARVIS shell commands" in help_out["output"]
    assert help_out["prompt"] == "jarvis@vos:/$ "

    ps = jvk_command({"action": "shell_exec", "line": "ps"})
    assert "PID" in ps["output"]

    mem = jvk_command({"action": "shell_exec", "line": "mem"})
    assert "frames" in mem["output"] and "allocator=" in mem["output"]

    ipc = jvk_command({"action": "shell_exec", "line": "ipc"})
    assert "agent-bus" in ipc["output"]

    devices = jvk_command({"action": "shell_exec", "line": "devices"})
    assert "DEVICE" in devices["output"]

    bad = jvk_command({"action": "shell_exec", "line": "nonsense"})
    assert bad["ok"] is False and "command not found" in bad["output"]


def test_shell_filesystem_commands() -> None:
    assert jvk_init({"boot": True}) == "ok"
    assert jvk_command({"action": "shell_exec", "line": "mkdir /sh"})["ok"]
    assert jvk_command(
        {"action": "shell_exec", "line": "write-file /sh/a.txt hello world"}
    )["ok"]
    cat = jvk_command({"action": "shell_exec", "line": "cat /sh/a.txt"})
    assert cat["output"].strip() == "hello world"
    assert "a.txt" in jvk_command({"action": "shell_exec", "line": "ls /sh"})["output"]
    assert jvk_command({"action": "shell_exec", "line": "cp /sh/a.txt /sh/b.txt"})["ok"]
    assert jvk_command({"action": "shell_exec", "line": "mv /sh/b.txt /sh/c.txt"})["ok"]
    assert jvk_command({"action": "shell_exec", "line": "rm /sh/c.txt"})["ok"]
    assert jvk_command({"action": "shell_exec", "line": "cd /sh"})["cwd"] == "/sh"
    assert jvk_command({"action": "shell_exec", "line": "pwd"})["output"].strip() == "/sh"
    snap = jvk_snapshot()["shell"]
    assert snap["cwd"] == "/sh" and snap["prompt"] == "jarvis@vos:/sh$ "


def test_shell_process_and_ipc_commands() -> None:
    assert jvk_init({"boot": True}) == "ok"
    pid = jvk_command({"action": "create_process", "name": "worker", "burst": 4})["pid"]
    ps = jvk_command({"action": "shell_exec", "line": "ps"})
    assert "worker" in ps["output"]
    alloc = jvk_command({"action": "shell_exec", "line": f"alloc {pid} 2"})
    assert alloc["ok"] is True and "allocated 2 page" in alloc["output"]
    assert jvk_command({"action": "shell_exec", "line": f"write {pid} 0 42"})["ok"]
    got = jvk_command({"action": "shell_exec", "line": f"read {pid} 0"})
    assert "= 42" in got["output"]

    send = jvk_command({"action": "shell_exec", "line": "send agent-bus hello"})
    assert send["ok"] is True
    recv = jvk_command({"action": "shell_exec", "line": "recv agent-bus"})
    assert recv["output"].strip() == "hello"

    sched = jvk_command({"action": "shell_exec", "line": "sched"})
    assert "algo=" in sched["output"]
    tick = jvk_command({"action": "shell_exec", "line": "tick 3"})
    assert tick["ok"] is True and "advanced 3 tick" in tick["output"]


def test_shell_shutdown_action() -> None:
    assert jvk_init({"boot": True}) == "ok"
    out = jvk_command({"action": "shell_exec", "line": "shutdown"})
    assert out["ok"] is True and out["exit"] is True
    assert jvk_snapshot()["shutdown"] is True


def test_config_includes_ipc_section() -> None:
    with TestClient(app) as client:
        body = client.get("/api/config").json()
        assert body["ipc"]["queueCapacity"] == 8
        assert body["ipc"]["seed"] is True


def test_jvk_shutdown() -> None:
    jvk_shutdown()
    snap = jvk_snapshot()
    assert snap["shutdown"] is True