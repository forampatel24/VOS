"""
backend/main.py

FastAPI bridge. Loads the C kernel at startup (ctypes) and exposes the
typed REST surface plus the WebSocket channel hub for the desktop.
All state originates in the C kernel; the bridge never fabricates data.
"""

from __future__ import annotations

from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, ConfigDict

from backend.routers.agent_runtime import router as agent_runtime_router
from backend.routers.agents import (
    register_stored_agents,
    router as agents_router,
)
from backend.routers.auth import router as auth_router
from backend.routers.devices import router as devices_router
from backend.routers.filesystem import router as filesystem_router
from backend.routers.interrupts import router as interrupts_router
from backend.routers.ipc import router as ipc_router
from backend.routers.memory import router as memory_router
from backend.routers.processes import router as processes_router
from backend.routers.scheduler import router as scheduler_router
from backend.routers.shell import router as shell_router
from backend.routers.system import (
    router as system_router,
    settings_router,
)
from backend.services import auth_store, settings_store
from backend.services.agent_store import init_db
from backend.services.config_loader import load_config
from backend.services.kernel_loader import (
    jvk_command,
    jvk_init,
    jvk_logs,
    jvk_shutdown,
    jvk_snapshot,
    jvk_tick,
)
from backend.ws.hub import handle_socket, hub


class CommandRequest(BaseModel):
    # Accept flat extra fields; the kernel expects one flat command dict,
    # so both {"action": X, "program": [...]} and the legacy nested
    # {"action": X, "data": {...}} shape are forwarded unchanged.
    model_config = ConfigDict(extra="allow")

    action: str


@asynccontextmanager
async def lifespan(_app: FastAPI):
    settings_store.init_db()
    auth_store.init_db()
    # config/*.json describe the virtual hardware the kernel boots on.
    # Persisted settings override the boot-time scheduler/memory policy.
    result = jvk_init(settings_store.apply_boot_config(load_config()))
    if result != "ok":
        raise RuntimeError(f"kernel failed to boot: {result}")
    init_db()
    register_stored_agents()
    await hub.start()
    yield
    await hub.stop()
    jvk_shutdown()


app = FastAPI(
    title="JARVIS OS Bridge",
    version="0.2.0",
    description=(
        "Typed REST and WebSocket bridge in front of the JARVIS C kernel. "
        "Every operation is forwarded through the kernel JSON ABI."
    ),
    lifespan=lifespan,
)

app.include_router(auth_router, prefix="/api/auth", tags=["auth"])
app.include_router(system_router, prefix="/api/system", tags=["system"])
app.include_router(settings_router, prefix="/api", tags=["settings"])
app.include_router(processes_router, prefix="/api/processes", tags=["processes"])
app.include_router(scheduler_router, prefix="/api", tags=["scheduler"])
app.include_router(memory_router, prefix="/api", tags=["memory"])
app.include_router(filesystem_router, prefix="/api", tags=["filesystem"])
app.include_router(devices_router, prefix="/api", tags=["devices"])
app.include_router(interrupts_router, prefix="/api", tags=["interrupts"])
app.include_router(ipc_router, prefix="/api", tags=["ipc"])
app.include_router(shell_router, prefix="/api", tags=["shell"])
app.include_router(agents_router, prefix="/api/agents", tags=["agents"])
app.include_router(
    agent_runtime_router, prefix="/api/agents", tags=["agent-runtime"]
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/health", tags=["system"])
def health() -> dict:
    return {"ok": True, "kernel": jvk_snapshot()}


@app.get("/api/snapshot", tags=["system"])
def snapshot() -> dict:
    return jvk_snapshot()


@app.get("/api/config", tags=["system"])
def config() -> dict:
    return load_config()


@app.post("/api/command", tags=["system"])
def command(req: CommandRequest) -> dict:
    fields = req.model_dump(exclude={"action"})
    nested = fields.pop("data", None)
    if isinstance(nested, dict):
        # Legacy nested shape: {"action": X, "data": {...}}
        payload = {"action": req.action, **nested, **fields}
    else:
        # Flat shape; "data" may legitimately be a string (device I/O).
        payload = {"action": req.action, **fields}
        if nested is not None:
            payload["data"] = nested
    return jvk_command(payload)


@app.get("/api/tick", tags=["system"])
def tick() -> dict:
    jvk_tick()
    return {"ok": True, "snapshot": jvk_snapshot()}


@app.get("/api/logs", tags=["system"])
def logs(since: int = 0) -> dict:
    return jvk_logs(max(0, since))


@app.websocket("/ws")
async def ws_all(ws: WebSocket):
    await handle_socket(ws, "all")


@app.websocket("/ws/{channel}")
async def ws_channel(ws: WebSocket, channel: str):
    await handle_socket(ws, channel)
