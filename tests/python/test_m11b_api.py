from __future__ import annotations

import pytest
from fastapi.testclient import TestClient

from backend.main import app


@pytest.fixture()
def client(tmp_path, monkeypatch):
    monkeypatch.setenv("JARVIS_AGENT_DB", str(tmp_path / "bridge.db"))
    monkeypatch.delenv("JARVIS_ADMIN_PASSWORD", raising=False)
    with TestClient(app) as test_client:
        yield test_client


def test_login_requires_admin_passcode(client: TestClient) -> None:
    ok = client.post(
        "/api/auth/login", json={"username": "admin", "password": "jarvis"}
    )
    assert ok.status_code == 200
    body = ok.json()
    assert body["ok"] is True
    assert body["user"]["username"] == "admin"
    assert body["user"]["role"] == "admin"
    assert "password_hash" not in body["user"]

    wrong = client.post(
        "/api/auth/login", json={"username": "admin", "password": "wrong"}
    )
    assert wrong.status_code == 401
    missing = client.post("/api/auth/login", json={"username": "admin"})
    assert missing.status_code == 422


def test_restart_reboots_kernel_and_keeps_settings(client: TestClient) -> None:
    client.patch("/api/settings", json={"scheduler": {"algorithm": "priority"}})
    created = client.post(
        "/api/processes", json={"name": "pre-reboot", "priority": 3, "burst_time": 2}
    ).json()
    assert created["ok"] is True

    halted = client.post("/api/system/shutdown")
    assert halted.status_code == 200
    assert client.get("/api/snapshot").json()["shutdown"] is True

    restarted = client.post("/api/system/restart")
    assert restarted.status_code == 200
    snapshot = restarted.json()["snapshot"]
    assert snapshot["booted"] is True
    assert snapshot["shutdown"] is False
    assert snapshot["scheduler"]["algo"] == "priority"
    assert client.get("/api/snapshot").json()["booted"] is True

    login = client.post(
        "/api/auth/login", json={"username": "admin", "password": "jarvis"}
    )
    assert login.status_code == 200
