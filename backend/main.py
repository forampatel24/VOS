"""
backend/main.py

FastAPI bridge. Loads the C kernel at startup (ctypes), exposes the
M1-M3b API surface (health + command proxy + snapshot + logs + tick)
and a WebSocket push channel (/ws) for the Electron/React desktop.
All state originates in the C kernel; the bridge never fabricates data.
"""

from __future__ import annotations

import asyncio
import json
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, ConfigDict

from backend.services.kernel_loader import (
    jvk_command,
    jvk_init,
    jvk_logs,
    jvk_shutdown,
    jvk_snapshot,
    jvk_tick,
)


class CommandRequest(BaseModel):
    # Accept flat extra fields; the kernel expects one flat command dict,
    # so both {"action": X, "program": [...]} and the legacy nested
    # {"action": X, "data": {...}} shape are forwarded unchanged.
    model_config = ConfigDict(extra="allow")

    action: str


@asynccontextmanager
async def lifespan(_app: FastAPI):
    result = jvk_init({"boot": True})
    if result != "ok":
        raise RuntimeError(f"kernel failed to boot: {result}")
    yield
    jvk_shutdown()


app = FastAPI(title="JARVIS OS Bridge", version="0.1.0", lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/health")
def health() -> dict:
    return {"ok": True, "kernel": jvk_snapshot()}


@app.get("/api/snapshot")
def snapshot() -> dict:
    return jvk_snapshot()


@app.post("/api/command")
def command(req: CommandRequest) -> dict:
    fields = req.model_dump(exclude={"action"})
    payload = {"action": req.action, **fields.pop("data", {}), **fields}
    return jvk_command(payload)


@app.get("/api/tick")
def tick() -> dict:
    jvk_tick()
    return {"ok": True, "snapshot": jvk_snapshot()}


@app.get("/api/logs")
def logs(since: int = 0) -> dict:
    return jvk_logs(since)


@app.websocket("/ws")
async def ws_endpoint(ws: WebSocket):
    await ws.accept()
    log_cursor = 0
    try:
        while True:
            snap = jvk_snapshot()
            log_data = jvk_logs(log_cursor)
            entries = log_data.get("logs", [])
            if entries:
                log_cursor = max(e["index"] for e in entries) + 1

            await ws.send_text(
                json.dumps(
                    {
                        "type": "update",
                        "snapshot": snap,
                        "logs": entries,
                    }
                )
            )
            # Check for client messages (e.g. tick requests) without blocking the push loop
            try:
                msg = await asyncio.wait_for(ws.receive_text(), timeout=0.5)
                try:
                    data = json.loads(msg)
                    if data.get("action") == "tick":
                        jvk_tick()
                    elif data.get("action") == "command":
                        # Allow WS to proxy commands as well (optional fast path)
                        result = jvk_command(data.get("payload", {}))
                        await ws.send_text(json.dumps({"type": "command_result", "result": result}))
                except json.JSONDecodeError:
                    pass
            except asyncio.TimeoutError:
                pass

            # Gentle background tick so the desktop looks alive (clock, scheduler)
            # Only when clients are connected — keeps demo lively without polling REST
            # 2.0s + 0.5s wait = ~2.5s per update so interrupts stay visible for demo
            jvk_tick()
            await asyncio.sleep(2.0)
    except WebSocketDisconnect:
        pass
    except Exception:
        try:
            await ws.close()
        except Exception:
            pass
