"""
backend/main.py

FastAPI bridge. Loads the C kernel at startup (ctypes), exposes a small
M1 API surface (health + command proxy), and shuts the kernel down on exit.
"""

from __future__ import annotations

from contextlib import asynccontextmanager

from fastapi import FastAPI
from pydantic import BaseModel

from backend.services.kernel_loader import (
    jvk_command,
    jvk_init,
    jvk_logs,
    jvk_shutdown,
    jvk_snapshot,
    jvk_tick,
)


class CommandRequest(BaseModel):
    action: str
    # optional fields forwarded as-is (agent, message, pid, ...)
    data: dict = {}


@asynccontextmanager
async def lifespan(_app: FastAPI):
    result = jvk_init({"boot": True})
    if result != "ok":
        raise RuntimeError(f"kernel failed to boot: {result}")
    yield
    jvk_shutdown()


app = FastAPI(title="JARVIS OS Bridge", version="0.1.0", lifespan=lifespan)


@app.get("/health")
def health() -> dict:
    return {"ok": True, "kernel": jvk_snapshot()}


@app.post("/api/command")
def command(req: CommandRequest) -> dict:
    payload = {"action": req.action, **req.data}
    return jvk_command(payload)


@app.get("/api/tick")
def tick() -> dict:
    jvk_tick()
    return {"ok": True, "snapshot": jvk_snapshot()}


@app.get("/api/logs")
def logs(since: int = 0) -> dict:
    return jvk_logs(since)