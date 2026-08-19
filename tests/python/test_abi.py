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