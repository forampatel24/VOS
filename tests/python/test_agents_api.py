from __future__ import annotations

from fastapi.testclient import TestClient

from backend.main import app

PROFILE = {
    "name": "Ledger Agent",
    "role": "Reconcile virtual ledgers",
    "personality": "Careful",
    "color": "#60a5fa",
    "icon": "book",
    "tools": ["read_file", "compute_total"],
    "priority": 6,
    "mem_pages": 3,
}


def test_profiles_persist_across_boot(tmp_path, monkeypatch) -> None:
    monkeypatch.setenv("JARVIS_AGENT_DB", str(tmp_path / "agents.db"))
    with TestClient(app) as client:
        created = client.post("/api/agents", json=PROFILE)
        assert created.status_code == 200
        profile = created.json()["profile"]
        key = profile["key"]
        assert key.startswith("custom-")
        agents = client.get("/api/snapshot").json()["agents"]
        assert any(d["key"] == key for d in agents["definitions"])

    with TestClient(app) as client:
        listed = client.get("/api/agents").json()["profiles"]
        assert [p["key"] for p in listed] == [key]
        agents = client.get("/api/snapshot").json()["agents"]
        assert any(d["key"] == key and not d["builtin"] for d in agents["definitions"])
        deleted = client.delete(f"/api/agents/{profile['id']}")
        assert deleted.status_code == 200
        agents = client.get("/api/snapshot").json()["agents"]
        assert all(d["key"] != key for d in agents["definitions"])
        assert client.get("/api/agents").json()["profiles"] == []


def test_running_profile_cannot_be_deleted(tmp_path, monkeypatch) -> None:
    monkeypatch.setenv("JARVIS_AGENT_DB", str(tmp_path / "agents.db"))
    with TestClient(app) as client:
        profile = client.post("/api/agents", json=PROFILE).json()["profile"]
        launched = client.post(
            "/api/command",
            json={"action": "agent_launch", "key": profile["key"]},
        ).json()
        assert launched["ok"] is True
        blocked = client.delete(f"/api/agents/{profile['id']}")
        assert blocked.status_code == 409
        killed = client.post(
            "/api/command",
            json={"action": "agent_kill", "pid": launched["pid"]},
        ).json()
        assert killed["ok"] is True
        assert client.delete(f"/api/agents/{profile['id']}").status_code == 200


def test_profile_validation_and_missing(tmp_path, monkeypatch) -> None:
    monkeypatch.setenv("JARVIS_AGENT_DB", str(tmp_path / "agents.db"))
    with TestClient(app) as client:
        assert client.post("/api/agents", json={"name": ""}).status_code == 422
        assert client.put("/api/agents/999", json=PROFILE).status_code == 404
        assert client.delete("/api/agents/999").status_code == 404
