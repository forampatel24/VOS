from __future__ import annotations

import pytest
from fastapi.testclient import TestClient

from backend.main import app


@pytest.fixture()
def client(tmp_path, monkeypatch):
    monkeypatch.setenv("JARVIS_AGENT_DB", str(tmp_path / "bridge.db"))
    with TestClient(app) as test_client:
        yield test_client


def test_settings_persist_and_apply_at_boot(tmp_path, monkeypatch) -> None:
    monkeypatch.setenv("JARVIS_AGENT_DB", str(tmp_path / "bridge.db"))
    with TestClient(app) as client:
        current = client.get("/api/settings")
        assert current.status_code == 200
        defaults = current.json()["settings"]
        assert defaults["scheduler"]["algorithm"] == "round_robin"
        assert defaults["memory"]["allocator"] == "first_fit"

        updated = client.patch(
            "/api/settings",
            json={
                "scheduler": {"algorithm": "priority"},
                "memory": {"allocator": "best_fit", "replacement": "lru"},
            },
        )
        assert updated.status_code == 200
        body = updated.json()
        assert body["persisted"] is True
        assert "memory" in body["applied"]
        assert "scheduler.algorithm" in body["applied"]

        snapshot = client.get("/api/snapshot").json()
        assert snapshot["scheduler"]["algo"] == "priority"
        assert snapshot["memory"]["config"]["allocator"] == "best_fit"
        assert snapshot["memory"]["config"]["replacement"] == "lru"

        invalid = client.patch(
            "/api/settings",
            json={"scheduler": {"algorithm": "bogus"}},
        )
        assert invalid.status_code == 422
        empty = client.patch("/api/settings", json={})
        assert empty.status_code == 422

    with TestClient(app) as client:
        snapshot = client.get("/api/snapshot").json()
        assert snapshot["scheduler"]["algo"] == "priority"
        assert snapshot["memory"]["config"]["allocator"] == "best_fit"
        stored = client.get("/api/system/settings").json()
        assert stored["persisted"] is True


def test_process_memory_scheduler_routes(client: TestClient) -> None:
    created = client.post(
        "/api/processes",
        json={"name": "api-worker", "priority": 7, "burst_time": 4},
    ).json()
    assert created["ok"] is True
    pid = created["pid"]

    listed = client.get("/api/processes").json()
    assert pid in listed["queues"]["ready"]
    detail = client.get(f"/api/processes/{pid}").json()
    assert detail["process"]["name"] == "api-worker"
    assert client.get("/api/processes/999").status_code == 404

    assert client.post(f"/api/processes/{pid}/suspend").json()["ok"] is True
    assert pid in client.get("/api/snapshot").json()["queues"]["suspended"]
    assert client.post(f"/api/processes/{pid}/resume").json()["ok"] is True

    allocated = client.post(
        "/api/memory/alloc", json={"pid": pid, "pages": 2}
    ).json()
    assert allocated["ok"] is True and allocated["frames"] == [0, 1]
    written = client.post(
        "/api/memory/write", json={"pid": pid, "addr": 3, "value": 88}
    ).json()
    assert written["ok"] is True
    read = client.post(
        "/api/memory/read", json={"pid": pid, "addr": 3}
    ).json()
    assert read["value"] == 88
    configured = client.patch(
        "/api/memory/config",
        json={"allocator": "worst_fit", "replacement": "fifo"},
    ).json()
    assert configured["allocator"] == "worst_fit"

    assert client.put(
        "/api/scheduler", json={"algo": "sjf"}
    ).json()["algo"] == "sjf"
    scheduler = client.get("/api/scheduler").json()
    assert scheduler["algo"] == "sjf"
    assert scheduler["next"] == pid
    cpu = client.get("/api/cpu").json()
    assert "registers" in cpu["cpu"]

    assert client.delete(f"/api/processes/{pid}").json()["ok"] is True


def test_filesystem_routes(client: TestClient) -> None:
    assert client.post(
        "/api/filesystem/directories", json={"path": "/Temp/m10b"}
    ).json()["ok"] is True
    created = client.post(
        "/api/filesystem/entries",
        json={"path": "/Temp/m10b/a.txt", "content": "hello"},
    ).json()
    assert created["ok"] is True

    read = client.post(
        "/api/filesystem/read", json={"path": "/Temp/m10b/a.txt"}
    ).json()
    assert read["content"] == "hello"
    assert client.post(
        "/api/filesystem/write",
        json={"path": "/Temp/m10b/a.txt", "content": " world", "append": True},
    ).json()["ok"] is True
    assert client.get(
        "/api/filesystem/stat", params={"path": "/Temp/m10b/a.txt"}
    ).json()["entry"]["size"] == 11

    renamed = client.post(
        "/api/filesystem/rename",
        json={"path": "/Temp/m10b/a.txt", "new_name": "b.txt"},
    ).json()
    assert renamed["ok"] is True
    assert client.post(
        "/api/filesystem/copy",
        json={"src": "/Temp/m10b/b.txt", "dst": "/Temp/m10b/c.txt"},
    ).json()["ok"] is True
    assert client.post(
        "/api/filesystem/move",
        json={"src": "/Temp/m10b/c.txt", "dst": "/Temp/m10b/d.txt"},
    ).json()["ok"] is True
    found = client.post(
        "/api/filesystem/search", json={"pattern": "b.txt"}
    ).json()
    assert any(e["name"] == "b.txt" for e in found["entries"])

    listing = client.get(
        "/api/filesystem/entries", params={"path": "/Temp/m10b"}
    ).json()
    assert {e["name"] for e in listing["entries"]} >= {"b.txt", "d.txt"}
    assert client.delete(
        "/api/filesystem/entries", params={"path": "/Temp/m10b/d.txt"}
    ).json()["ok"] is True
    assert client.get("/api/filesystem/disk").json()["disk"]["used_blocks"] > 0


def test_device_interrupt_ipc_and_shell_routes(client: TestClient) -> None:
    assert client.post(
        "/api/devices/keyboard/write", json={"data": "typed"}
    ).json()["bytes"] == 5
    assert client.post(
        "/api/devices/keyboard/read", json={}
    ).json()["data"] == "typed"
    assert client.post("/api/devices/disk/flush").status_code == 200
    device = client.get("/api/devices/display").json()["device"]
    assert device["key"] == "display"

    assert client.post(
        "/api/interrupts", json={"irq": "software", "source": "api"}
    ).json()["ok"] is True
    interrupts = client.get("/api/interrupts").json()
    assert interrupts["ok"] is True
    assert interrupts["interrupts"]["pending"] >= 1
    assert "error_manager" in interrupts

    assert client.post(
        "/api/ipc/queues", json={"queue": "apiq", "capacity": 2}
    ).json()["ok"] is True
    assert client.post(
        "/api/ipc/queues/apiq/send", json={"data": "hello"}
    ).json()["ok"] is True
    assert client.post(
        "/api/ipc/queues/apiq/recv", json={}
    ).json()["data"] == "hello"
    owner = client.post(
        "/api/processes", json={"name": "ipc-owner"}
    ).json()["pid"]
    assert client.post(
        "/api/ipc/segments",
        json={"segment": "apiseg", "size": 32, "pid": owner},
    ).json()["ok"] is True
    assert client.post(
        "/api/ipc/segments/apiseg/write",
        json={"data": "shared", "pid": owner},
    ).json()["bytes"] == 6
    assert client.post(
        "/api/ipc/segments/apiseg/read", json={"pid": owner}
    ).json()["data"] == "shared"
    assert client.post(
        "/api/ipc/mutexes", json={"mutex": "apim"}
    ).json()["ok"] is True
    assert client.post(
        "/api/ipc/mutexes/apim/lock", json={}
    ).json()["ok"] is True
    assert client.post(
        "/api/ipc/mutexes/apim/unlock", json={}
    ).json()["ok"] is True
    assert client.get("/api/ipc/deadlock").json()["deadlock"] is False

    shell = client.post("/api/shell/exec", json={"line": "pwd"}).json()
    assert shell["ok"] is True and shell["output"].strip() == "/"
    assert client.get("/api/shell").json()["shell"]["cwd"] == "/"


def test_agent_runtime_routes(client: TestClient) -> None:
    launched = client.post(
        "/api/agents/launch", json={"agent": "finance"}
    ).json()
    assert launched["ok"] is True
    pid = launched["pid"]

    task = client.post(
        f"/api/agents/instances/{pid}/tasks",
        json={"title": "API review", "input": "budget 100 20 5"},
    ).json()
    assert task["ok"] is True
    task_id = task["task_id"]

    runtime = client.get("/api/agents/runtime").json()
    assert runtime["ok"] is True
    assert any(i["pid"] == pid for i in runtime["instances"])
    assert any(t["id"] == task_id for t in runtime["tasks"])

    client.post("/api/system/tick", json={"count": 40})
    tasks = client.get("/api/agents/tasks", params={"pid": pid}).json()
    done = next(t for t in tasks["tasks"] if t["id"] == task_id)
    assert done["stage"] == "DONE" and done["progress"] == 100

    report = client.post(
        "/api/filesystem/read",
        json={"path": f"/Agents/finance/report-{task_id}.md"},
    ).json()
    assert report["ok"] is True and len(report["content"]) > 20

    assert client.delete(f"/api/agents/instances/{pid}").json()["ok"] is True
    runtime = client.get("/api/agents/runtime").json()
    assert all(i["pid"] != pid for i in runtime["instances"])


def test_websocket_channels_and_commands(client: TestClient) -> None:
    with client.websocket_connect("/ws") as ws:
        frame = ws.receive_json()
        assert frame["type"] == "update"
        assert frame["channel"] == "all"
        assert "snapshot" in frame and "logs" in frame
        ws.send_json({"action": "ping"})
        while True:
            reply = ws.receive_json()
            if reply["type"] == "pong":
                break
        ws.send_json({"action": "snapshot"})
        while True:
            frame = ws.receive_json()
            if frame["type"] == "update":
                break
        assert frame["snapshot"]["booted"] is True

    with client.websocket_connect("/ws/memory") as ws:
        frame = ws.receive_json()
        assert frame["channel"] == "memory"
        assert {"config", "stats", "frame_map"} <= set(frame["data"])
        pushed = ws.receive_json()
        assert pushed["type"] == "update"
        assert pushed["uptime_ticks"] >= frame["uptime_ticks"]

    with client.websocket_connect("/ws/notifications") as ws:
        frame = ws.receive_json()
        assert frame["channel"] == "notifications"
        assert all("level" in entry for entry in frame["data"]["notifications"])

    with client.websocket_connect("/ws/voice") as ws:
        status = ws.receive_json()
        assert status["type"] == "status"
        assert status["available"] is False

    with client.websocket_connect("/ws/nope") as ws:
        error = ws.receive_json()
        assert error["type"] == "error"


def test_legacy_endpoints_remain_compatible(client: TestClient) -> None:
    assert client.get("/health").json()["kernel"]["booted"] is True
    assert client.get("/api/snapshot").json()["booted"] is True
    assert client.get("/api/config").json()["machine"]["chipset"] == "JARVIS-VC1"
    assert client.post(
        "/api/command", json={"action": "ping"}
    ).json()["kernel"] == "c-native"
    assert client.get("/api/tick").json()["ok"] is True
    assert client.get("/api/logs").json()["logs"]
    info = client.get("/api/system/info").json()
    assert info["counts"]["frames_total"] >= 1
