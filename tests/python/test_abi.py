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