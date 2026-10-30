"""
backend/routers/system.py

System information, settings, ticks, logs and shutdown. Settings persist in
SQLite and runtime-changeable values are applied to the kernel immediately.
"""

from __future__ import annotations

import copy

from fastapi import APIRouter, HTTPException

from backend.routers.common import kernel_action, with_ok
from backend.schemas.common import (
    KernelReply,
    KernelSnapshot,
    LogsResponse,
    TickRequest,
    TickResponse,
)
from backend.schemas.system import (
    ConfigResponse,
    SettingsDocument,
    SettingsPatch,
    SettingsResponse,
    SettingsUpdateResponse,
    SystemInfo,
)
from backend.services import settings_store
from backend.services.config_loader import load_config
from backend.services.kernel_loader import (
    jvk_init,
    jvk_logs,
    jvk_shutdown,
    jvk_snapshot,
    kernel_lock,
)

router = APIRouter()


def _info() -> dict:
    snapshot = jvk_snapshot()
    config = load_config()
    memory = snapshot.get("memory", {})
    filesystem = snapshot.get("filesystem", {})
    agents = snapshot.get("agents", {})
    ipc = snapshot.get("ipc", {})
    devices = snapshot.get("devices", {})
    counts = {
        "processes": int(snapshot.get("processes", 0)),
        "frames_used": int(memory.get("stats", {}).get("frames_used", 0)),
        "frames_total": int(memory.get("config", {}).get("total_frames", 0)),
        "devices": int(devices.get("count", 0)),
        "ipc_queues": len(ipc.get("queues", [])),
        "agents_live": int(agents.get("stats", {}).get("live", 0)),
        "agents_definitions": int(
            agents.get("stats", {}).get("definitions", 0)
        ),
        "inodes_used": int(filesystem.get("stats", {}).get("inodes_used", 0)),
    }
    return with_ok(
        {
            "kernel": "c-native",
            "booted": snapshot.get("booted", False),
            "shutdown": snapshot.get("shutdown", False),
            "uptime_ticks": int(snapshot.get("uptime_ticks", 0)),
            "machine": snapshot.get("machine", {}),
            "clock": snapshot.get("clock", {}),
            "scheduler": snapshot.get("scheduler", {}),
            "config": config,
            "counts": counts,
        }
    )


@router.get("", response_model=SystemInfo)
@router.get("/info", response_model=SystemInfo)
def system_info() -> dict:
    return _info()


@router.get("/snapshot", response_model=KernelSnapshot)
def system_snapshot() -> dict:
    return jvk_snapshot()


@router.get("/config", response_model=ConfigResponse)
def system_config() -> dict:
    return {"ok": True, "config": load_config()}


@router.get("/logs", response_model=LogsResponse)
def system_logs(since: int = 0) -> dict:
    return jvk_logs(max(0, since))


@router.post("/tick", response_model=TickResponse)
def system_tick(payload: TickRequest) -> dict:
    result = kernel_action("tick", payload)
    if not result.get("ok"):
        raise HTTPException(status_code=400, detail=result.get("error"))
    return {"ok": True, "snapshot": jvk_snapshot()}


@router.post("/shutdown", response_model=KernelReply)
def system_shutdown() -> dict:
    return kernel_action("shutdown")


@router.post("/restart", response_model=TickResponse)
def system_restart() -> dict:
    """Reboot the kernel in place, re-applying persisted boot settings."""
    with kernel_lock():
        jvk_shutdown()
        result = jvk_init(settings_store.apply_boot_config(load_config()))
        if result != "ok":
            raise HTTPException(
                status_code=500, detail=f"kernel reboot failed: {result}"
            )
        # Custom Agent Studio profiles are durable; re-register them with the
        # fresh kernel registry after the built-ins have been seeded.
        from backend.routers.agents import register_stored_agents

        register_stored_agents()
        snapshot = jvk_snapshot()
    return {"ok": True, "snapshot": snapshot}


def _merge(base: dict, extra: dict) -> dict:
    for key, value in extra.items():
        if isinstance(value, dict) and isinstance(base.get(key), dict):
            _merge(base[key], value)
        else:
            base[key] = value
    return base


def _apply_patch(patch: SettingsPatch) -> SettingsUpdateResponse:
    current, _ = settings_store.get_settings()
    merged = copy.deepcopy(current.model_dump())
    _merge(merged, patch.model_dump(exclude_none=True))
    updated = SettingsDocument(**merged)

    applied: list[str] = []
    restart: list[str] = []
    try:
        if updated.memory != current.memory:
            settings_store.apply_memory(updated)
            applied.append("memory")
        if updated.scheduler.algorithm != current.scheduler.algorithm:
            settings_store.apply_scheduler(updated)
            applied.append("scheduler.algorithm")
    except settings_store.SettingsError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc

    for field in (
        "quantum",
        "context_switch_overhead",
        "max_agents",
    ):
        if getattr(updated.scheduler, field) != getattr(
            current.scheduler, field
        ):
            restart.append(f"scheduler.{field}")

    settings_store.save_settings(updated)
    return SettingsUpdateResponse(
        settings=updated,
        persisted=True,
        applied=applied,
        restart_required=restart,
    )


@router.get("/settings", response_model=SettingsResponse)
def system_settings() -> dict:
    document, persisted = settings_store.get_settings()
    return {"ok": True, "settings": document, "persisted": persisted}


@router.patch("/settings", response_model=SettingsUpdateResponse)
def patch_system_settings(payload: SettingsPatch) -> SettingsUpdateResponse:
    return _apply_patch(payload)


def _settings_alias_router() -> APIRouter:
    alias = APIRouter()

    @alias.get("/settings", response_model=SettingsResponse)
    def get_alias() -> dict:
        return system_settings()

    @alias.patch("/settings", response_model=SettingsUpdateResponse)
    def patch_alias(payload: SettingsPatch) -> SettingsUpdateResponse:
        return _apply_patch(payload)

    return alias


settings_router = _settings_alias_router()
